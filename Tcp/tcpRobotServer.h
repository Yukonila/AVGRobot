#ifndef TCPROBOTSERVER_H
#define TCPROBOTSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QHash>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>

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

    // 下行指令（带 seq 与 ACK 重传）
    bool sendPath(int robotId, int taskId, const QList<QPointF> &path); // 0x03 路径下发
    bool sendControl(int robotId, const QString &cmd, int taskId = -1); // 0x06 控制(pause/resume/estop)

signals:
    void logMessage(const QString &msg, int level);
    void robotConnected(int robotId, const QString &peerIp);
    void robotDisconnected(int robotId);
    void robotHeartbeatTimeout(int robotId); // 心跳超时(判离线)
    void robotReported(int robotId, float x, float y, int battery, int status, bool hasPos);
    void robotAlarm(int robotId, int code, const QString &msg); // 0x05 告警上报
    void ackReceived(int seq, bool ok);    // 0xF0 ACK / 0xF1 NACK
    void ackTimeout(int seq);              // 重传耗尽

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onHeartbeat(); // 心跳超时检查

private:
    void handleLine(QTcpSocket *sock, const QByteArray &line);
    void touch(QTcpSocket *sock);
    int socketRobotId(QTcpSocket *sock) const;

    // 二进制帧分发（按功能码）
    void dispatchBinary(QTcpSocket *sock, quint8 func, const QByteArray &payload);
    void handleAlarm(QTcpSocket *sock, const QJsonObject &o);   // 0x05
    void handleAck(QTcpSocket *sock, const QJsonObject &o, quint8 func); // 0xF0/0xF1
    // 带 seq 的可靠下发：写入帧、登记 pending、等待 ACK；超时由 onHeartbeat 重传
    bool sendReliable(int robotId, quint8 func, const QJsonObject &obj);

    QTcpServer *m_server;
    QHash<QTcpSocket *, int> m_socketId;      // socket -> robotId
    QHash<QTcpSocket *, QByteArray> m_buffer; // socket -> 尚未拆包的字节
    QHash<int, QTcpSocket *> m_robotSock;     // robotId -> socket
    QHash<QTcpSocket *, qint64> m_lastSeen;   // socket -> 最后收到时间(ms)
    QHash<int, bool> m_robotLineLost;         // robotId -> 是否已判离线
    QTimer *m_heartTimer;
    int m_heartTimeoutMs;                     // 心跳超时阈值
    int m_ackTimeoutMs;                       // 下行指令 ACK 等待阈值
    int m_maxRetries;                         // ACK 超时最大重传次数

    // ACK 重传状态
    struct PendingCmd
    {
        int seq;
        int robotId;
        quint8 func;
        QJsonObject obj;
        qint64 sentAt;
        int retries;
    };
    QHash<int, PendingCmd> m_pending;         // seq -> 待确认指令
    int m_seq;                                // 单调递增序号
};

#endif // TCPROBOTSERVER_H
