// robotClient —— 模拟一台真实机器人，作为 TCP 客户端接入 RCS 服务端。
// 用法: ./robotClient [robotId] [host] [port] [周期ms]
//   注册后按周期以二进制帧(JSON负载)上报位置/电量/状态。
#include <QCoreApplication>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <cstdio>
#include <cmath>

static QByteArray makeFrame(const QJsonObject &obj)
{
    QByteArray d = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray b;
    b.append((char)0xAA).append((char)0x55);       // 帧头
    quint16 n = (quint16)d.size();
    b.append((char)(n >> 8)).append((char)(n & 0xff)); // 长度
    b.append((char)0x01);                           // 功能码 0x01 状态上报
    quint8 x = 0;
    x ^= (quint8)(n >> 8); x ^= (quint8)(n & 0xff);
    x ^= 0x01;
    for (int i = 0; i < d.size(); ++i) x ^= (quint8)d[i];
    b.append(d);
    b.append((char)x);                              // 异或校验
    b.append((char)0xDD).append((char)0xEE);        // 帧尾
    return b;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    int robotId = argc > 1 ? QString(argv[1]).toInt() : 1;
    QString host = argc > 2 ? QString(argv[2]) : QStringLiteral("127.0.0.1");
    int port = argc > 3 ? QString(argv[3]).toInt() : 8888;
    int period = argc > 4 ? QString(argv[4]).toInt() : 1000;

    QTcpSocket sock;
    int attempts = 0;
    QObject::connect(&sock, &QTcpSocket::connected, [&]()
    {
        attempts = 0;
        QJsonObject reg; reg["id"] = robotId;
        sock.write(QJsonDocument(reg).toJson(QJsonDocument::Compact) + "\n");
        std::printf("[client] 已连接 %s:%d, 注册 id=%d\n", qPrintable(host), port, robotId);
    });
    QObject::connect(&sock, &QTcpSocket::errorOccurred, [&](QAbstractSocket::SocketError)
    {
        std::printf("[client] 连接错误: %s\n", qPrintable(sock.errorString()));
    });
    QObject::connect(&sock, &QTcpSocket::disconnected, [&]()
    {
        std::printf("[client] 已断开\n");
        if (attempts < 5)
        {
            ++attempts;
            std::printf("[client] 第 %d 次重连...\n", attempts);
            QTimer::singleShot(2000, [&]() { sock.connectToHost(host, (quint16)port); });
        }
        else
        {
            std::printf("[client] 重连次数用尽，退出\n");
            app.quit();
        }
    });

    float x = 0.5f, y = 0.5f;
    int dir = 1;
    int battery = 100;

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]()
    {
        if (sock.state() != QAbstractSocket::ConnectedState)
            return;
        x += dir * 1.0f;
        if (x > 40.0f) dir = -1;
        if (x < 0.5f) dir = 1;
        --battery;                 // 掉电
        if (battery <= 0) battery = 100; // 简单重置演示
        QJsonObject o;
        o["x"] = x; o["y"] = y; o["battery"] = battery; o["status"] = 0;
        sock.write(makeFrame(o));
        sock.flush();
        std::printf("[client] id=%d pos=(%.1f,%.1f) batt=%d%%\n", robotId, x, y, battery);
    });

    timer.start(period);
    sock.connectToHost(host, (quint16)port);

    return app.exec();
}
