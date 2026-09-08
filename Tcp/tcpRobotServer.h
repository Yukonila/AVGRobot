#ifndef TCPROBOTSERVER_H
#define TCPROBOTSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QHash>
#include <QByteArray>

// ====== RCS 侧 TCP 服务端 ======
// 机器人作为 TCP 客户端接入。协议（按行）：
//   注册:   {"id":123}
//   状态:   {"x":1.5,"y":3.0,"battery":88,"status":0}   // status: RobotStatus int
// 服务端解析后以信号上报，由上层(主窗口/控制器)更新机器人。
class TcpRobotServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpRobotServer(QObject *parent = nullptr);
    ~TcpRobotServer() override;

    bool start(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;
    QList<int> connectedRobotIds() const { return m_socketId.values(); }

    // 向某台机器人下发 JSON(任务/控制指令留待扩展)
    bool sendToRobot(int robotId, const QJsonObject &obj);

signals:
    void logMessage(const QString &msg, int level);
    void robotConnected(int robotId, const QString &peerIp);
    void robotDisconnected(int robotId);
    void robotHeartbeatTimeout(int robotId); // 心跳超时(判离线)
    void robotReported(int robotId, float x, float y, int battery, int status, bool hasPos);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onHeartbeat(); // 心跳超时检查

private:
    void handleLine(QTcpSocket *sock, const QByteArray &line);
    void touch(QTcpSocket *sock);
    int socketRobotId(QTcpSocket *sock) const;

    QTcpServer *m_server;
    QHash<QTcpSocket *, int> m_socketId;      // socket -> robotId
    QHash<QTcpSocket *, QByteArray> m_buffer; // socket -> 尚未拆包的字节
    QHash<int, QTcpSocket *> m_robotSock;     // robotId -> socket
    QHash<QTcpSocket *, qint64> m_lastSeen;   // socket -> 最后收到时间(ms)
    QHash<int, bool> m_robotLineLost;         // robotId -> 是否已判离线
    QTimer *m_heartTimer;
    int m_heartTimeoutMs;                     // 心跳超时阈值
};

#endif // TCPROBOTSERVER_H
