#include "tcpRobotServer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QDateTime>

TcpRobotServer::TcpRobotServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_heartTimer(new QTimer(this))
    , m_heartTimeoutMs(3000)
{
    connect(m_server, &QTcpServer::newConnection, this, &TcpRobotServer::onNewConnection);
    m_heartTimer->setInterval(1000);
    connect(m_heartTimer, &QTimer::timeout, this, &TcpRobotServer::onHeartbeat);
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
    s->write(doc.toJson(QJsonDocument::Compact));
    s->write("\n");
    return s->flush() || s->bytesToWrite() >= 0;
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
            quint8 func = quint8(buf[4]);
            QByteArray payload = buf.mid(5, dlen);
            if (x == stored)
                handleLine(sock, payload); // payload 为 JSON(状态上报)
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
}
