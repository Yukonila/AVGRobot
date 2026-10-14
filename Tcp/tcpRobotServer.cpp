#include "tcpRobotServer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostAddress>
#include <QDateTime>
#include <QPointF>

// 组一个二进制帧: [AA55][len][func][data][xor][DDEE]
static QByteArray makeFrame(quint8 func, const QByteArray &d)
{
    QByteArray b;
    b.append((char)0xAA).append((char)0x55);
    quint16 n = (quint16)d.size();
    b.append((char)(n >> 8)).append((char)(n & 0xff));
    b.append((char)func);
    quint8 x = 0;
    x ^= (quint8)(n >> 8); x ^= (quint8)(n & 0xff); x ^= func;
    for (int i = 0; i < d.size(); ++i) x ^= (quint8)d[i];
    b.append(d);
    b.append((char)x);
    b.append((char)0xDD).append((char)0xEE);
    return b;
}


TcpRobotServer::TcpRobotServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_heartTimer(new QTimer(this))
    , m_heartTimeoutMs(3000)
{
    connect(m_server, &QTcpServer::newConnection, this, &TcpRobotServer::onNewConnection);
    m_heartTimer->setInterval(1000);
    connect(m_heartTimer, &QTimer::timeout, this, &TcpRobotServer::onHeartbeat);
    m_ackTimeoutMs = 3000;
    m_maxRetries = 3;
    m_seq = 0;
}

TcpRobotServer::~TcpRobotServer()
{
    stop();
}

bool TcpRobotServer::start(quint16 port)
{
    stop();
    if (!m_server->listen(QHostAddress::Any, port))
    {
        emit logMessage("[TCP] 监听失败: " + m_server->errorString(), 2);
        return false;
    }
    if (!m_heartTimer->isActive())
        m_heartTimer->start();
    emit logMessage("[TCP] 服务端已启动，监听端口 " + QString::number(m_server->serverPort()), 0);
    return true;
}

void TcpRobotServer::stop()
{
    if (m_heartTimer->isActive())
        m_heartTimer->stop();
    if (m_server->isListening())
        m_server->close();
    for (QTcpSocket *s : m_server->findChildren<QTcpSocket *>())
        s->deleteLater();
    m_buffer.clear();
    m_socketId.clear();
    m_robotSock.clear();
    m_lastSeen.clear();
    m_robotLineLost.clear();
}

bool TcpRobotServer::isListening() const
{
    return m_server->isListening();
}

quint16 TcpRobotServer::port() const
{
    return m_server->serverPort();
}

bool TcpRobotServer::sendToRobot(int robotId, const QJsonObject &obj)
{
    QTcpSocket *s = m_robotSock.value(robotId, nullptr);
    if (!s)
        return false;
    QJsonDocument doc(obj);
    s->write(makeFrame(0x02, doc.toJson(QJsonDocument::Compact))); // 0x02 任务下发
    return s->flush() || s->bytesToWrite() >= 0;
}

bool TcpRobotServer::sendPath(int robotId, int taskId, const QList<QPointF> &path)
{
    QJsonObject o;
    o["taskId"] = taskId;
    QJsonArray arr;
    for (const QPointF &p : path)
    {
        QJsonArray wp;
        wp.append(p.x());
        wp.append(p.y());
        arr.append(wp);
    }
    o["path"] = arr;
    o["count"] = arr.size();
    return sendReliable(robotId, 0x03, o); // 0x03 路径下发(需 ACK)
}

bool TcpRobotServer::sendControl(int robotId, const QString &cmd, int taskId)
{
    QJsonObject o;
    o["cmd"] = cmd; // pause / resume / estop
    if (taskId >= 0)
        o["taskId"] = taskId;
    return sendReliable(robotId, 0x06, o); // 0x06 控制指令(需 ACK)
}

bool TcpRobotServer::sendReliable(int robotId, quint8 func, const QJsonObject &obj)
{
    QTcpSocket *s = m_robotSock.value(robotId, nullptr);
    if (!s || s->state() != QAbstractSocket::ConnectedState)
        return false;
    int seq = ++m_seq;
    QJsonObject o = obj;
    o["seq"] = seq; // 让对端回 ACK 时原样带回
    QByteArray data = QJsonDocument(o).toJson(QJsonDocument::Compact);
    s->write(makeFrame(func, data));
    s->flush();
    PendingCmd pc;
    pc.seq = seq;
    pc.robotId = robotId;
    pc.func = func;
    pc.obj = o;
    pc.sentAt = QDateTime::currentMSecsSinceEpoch();
    pc.retries = 0;
    m_pending[seq] = pc;
    emit logMessage(QString("[TCP] 下发 0x%1 seq=%2 至机器人 %3，等待 ACK")
                        .arg(func, 0, 16)
                        .arg(seq)
                        .arg(robotId),
                    0);
    return true;
}

void TcpRobotServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection())
    {
        connect(sock, &QTcpSocket::readyRead, this, &TcpRobotServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &TcpRobotServer::onDisconnected);
        m_buffer[sock] = QByteArray();
    }
}

int TcpRobotServer::socketRobotId(QTcpSocket *sock) const
{
    return m_socketId.value(sock, -1);
}

void TcpRobotServer::touch(QTcpSocket *sock)
{
    m_lastSeen[sock] = QDateTime::currentMSecsSinceEpoch();
}

void TcpRobotServer::onReadyRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;
    m_buffer[sock] += sock->readAll();
    QByteArray &buf = m_buffer[sock];

    for (;;)
    {
        if (buf.size() < 4)
            break;

        bool isFrame = (quint8(buf[0]) == 0xAA && quint8(buf[1]) == 0x55);
        if (isFrame)
        {
            // 二进制帧: [AA55][len2B][func1B][data len][xor1B][DDEE]
            if (buf.size() < 6)
                break;
            quint16 dlen = (quint8(buf[2]) << 8) | quint8(buf[3]);
            int total = 8 + dlen;
            if (buf.size() < total)
                break; // 数据不完整，等下一包
            // 校验帧尾与异或
            if (!(quint8(buf[total - 2]) == 0xDD && quint8(buf[total - 1]) == 0xEE))
            {
                buf.remove(0, 2); // 丢弃非法的帧头，重新同步
                continue;
            }
            quint8 stored = quint8(buf[total - 3]);
            quint8 x = 0;
            for (int i = 2; i <= 4 + dlen; ++i)
                x ^= quint8(buf[i]); // 长度(2)+功能码(1)+数据(dlen)
            QByteArray payload = buf.mid(5, dlen);
            quint8 func = (quint8)buf[4];
            if (x == stored)
                dispatchBinary(sock, func, payload);
            else
                emit logMessage("[TCP] 帧校验失败(异或不符)", 2);
            buf.remove(0, total);
        }
        else
        {
            int nl = buf.indexOf('\n');
            if (nl < 0)
                break;
            QByteArray line = buf.left(nl).trimmed();
            buf.remove(0, nl + 1);
            if (!line.isEmpty())
                handleLine(sock, line);
        }
    }
}

void TcpRobotServer::dispatchBinary(QTcpSocket *sock, quint8 func, const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        if (func == 0x01)
        {
            // 兼容：0x01 可能直接是裸 JSON 行(无 seq 包装)
            Q_UNUSED(sock);
            return; // 由 handleLine 处理 (下行不应到这)
        }
        emit logMessage("[TCP] 二进制帧 JSON 解析失败 func=0x" + QString::number(func, 16), 2);
        return;
    }
    QJsonObject o = doc.object();
    switch (func)
    {
    case 0x01: // 状态上报(JSON 载荷)
        handleLine(sock, payload);
        break;
    case 0x05: // 告警上报
        handleAlarm(sock, o);
        break;
    case 0xF0: // ACK
    case 0xF1: // NACK
        handleAck(sock, o, func);
        break;
    default:
        emit logMessage("[TCP] 收到未处理功能码 0x" + QString::number(func, 16), 1);
        break;
    }
}

void TcpRobotServer::handleAlarm(QTcpSocket *sock, const QJsonObject &o)
{
    int id = o.value("id").toInt(socketRobotId(sock));
    int code = o.value("code").toInt(0);
    QString msg = o.value("msg").toString();
    touch(sock);
    emit robotAlarm(id, code, msg);
    emit logMessage(QString("[TCP] 机器人 %1 告警[code=%2]: %3").arg(id).arg(code).arg(msg), 1);
    // 回 ACK
    QTcpSocket *s = m_robotSock.value(id, nullptr);
    if (s)
        s->write(makeFrame(0xF0, QJsonDocument(QJsonObject{{"seq", o.value("seq")}, {"ok", true}}).toJson(QJsonDocument::Compact)));
}

void TcpRobotServer::handleAck(QTcpSocket *sock, const QJsonObject &o, quint8 func)
{
    Q_UNUSED(sock);
    int seq = o.value("seq").toInt(-1);
    bool ok = (func == 0xF0) && o.value("ok").toBool(true);
    if (seq >= 0)
        m_pending.remove(seq); // 确认收到，移除待重传
    emit ackReceived(seq, ok);
    emit logMessage(QString("[TCP] %1 seq=%2").arg(ok ? "ACK" : "NACK").arg(seq), 0);
}

void TcpRobotServer::handleLine(QTcpSocket *sock, const QByteArray &line)
{
    touch(sock); // 任何有效报文都刷新心跳时间
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return; // 非 JSON 忽略(兼容握手文本)
    QJsonObject o = doc.object();

    int id = o.value("id").toInt(-1);
    if (id >= 0 && !m_socketId.contains(sock))
    {
        // 首次含 id 视为注册
        m_socketId[sock] = id;
        m_robotSock[id] = sock;
        QString peer = sock->peerAddress().toString();
        emit robotConnected(id, peer);
        emit logMessage("[TCP] 机器人 " + QString::number(id) + " 接入 (" + peer + ")", 0);
        return;
    }
    id = (id >= 0) ? id : socketRobotId(sock);
    if (id < 0)
        return; // 尚未注册
    // 曾判离线，现在有报文 → 视为恢复在线
    if (m_robotLineLost.value(id, false))
    {
        m_robotLineLost[id] = false;
        emit logMessage("[TCP] 机器人 " + QString::number(id) + " 已恢复上报", 0);
    }
    float x = (float)o.value("x").toDouble(0.0);
    float y = (float)o.value("y").toDouble(0.0);
    int battery = o.value("battery").toInt(-1);
    int status = o.value("status").toInt(-1000);
    bool hasPos = (o.contains("x") && o.contains("y"));
    if (hasPos || battery >= 0 || status >= 0)
        emit robotReported(id, x, y, battery, status, hasPos);
}

void TcpRobotServer::onDisconnected()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;
    int id = socketRobotId(sock);
    if (id >= 0)
    {
        m_robotSock.remove(id);
        m_robotLineLost.remove(id);
        emit robotDisconnected(id);
        emit logMessage("[TCP] 机器人 " + QString::number(id) + " 断开", 1);
    }
    m_socketId.remove(sock);
    m_buffer.remove(sock);
    m_lastSeen.remove(sock);
    sock->deleteLater();
}

void TcpRobotServer::onHeartbeat()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (QTcpSocket *sock : m_socketId.keys())
    {
        int id = m_socketId.value(sock, -1);
        if (id < 0)
            continue;
        qint64 last = m_lastSeen.value(sock, 0);
        if (last > 0 && (now - last) > m_heartTimeoutMs && !m_robotLineLost.value(id, false))
        {
            m_robotLineLost[id] = true;
            emit robotHeartbeatTimeout(id);
            emit logMessage("[TCP] 机器人 " + QString::number(id) + " 心跳超时，判离线", 2);
        }
    }
    // 周期性向所有已接入机器人发送心跳帧(0x04)
    for (QTcpSocket *sock : m_socketId.keys())
    {
        if (sock && sock->state() == QAbstractSocket::ConnectedState)
        {
            sock->write(makeFrame(0x04, QByteArray()));
            sock->flush();
        }
    }

    // 下行指令 ACK 超时重传
    for (auto it = m_pending.begin(); it != m_pending.end();)
    {
        PendingCmd &pc = it.value();
        QTcpSocket *s = m_robotSock.value(pc.robotId, nullptr);
        if (!s || s->state() != QAbstractSocket::ConnectedState)
        {
            emit ackTimeout(pc.seq);
            emit logMessage(QString("[TCP] 下发 seq=%1 机器人 %2 已断开，放弃重传")
                                .arg(pc.seq).arg(pc.robotId), 2);
            it = m_pending.erase(it);
            continue;
        }
        if (now - pc.sentAt > m_ackTimeoutMs)
        {
            if (pc.retries >= m_maxRetries)
            {
                emit ackTimeout(pc.seq);
                emit logMessage(QString("[TCP] 下发 seq=%1 至机器人 %2 重试 %3 次仍无 ACK，放弃")
                                    .arg(pc.seq).arg(pc.robotId).arg(pc.retries), 2);
                it = m_pending.erase(it);
                continue;
            }
            // 重传同一帧
            QByteArray data = QJsonDocument(pc.obj).toJson(QJsonDocument::Compact);
            s->write(makeFrame(pc.func, data));
            s->flush();
            pc.sentAt = now;
            ++pc.retries;
            emit logMessage(QString("[TCP] 重传 seq=%1 (第%2次) 至机器人 %3")
                                .arg(pc.seq).arg(pc.retries).arg(pc.robotId), 1);
        }
        ++it;
    }
}
