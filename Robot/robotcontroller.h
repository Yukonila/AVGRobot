#ifndef ROBOTCONTROLLER_H
#define ROBOTCONTROLLER_H

#include <QObject>
#include "robotManager.h"

class RobotController : public QObject
{
    Q_OBJECT

public:
    explicit RobotController(QObject *parent = nullptr);
    ~RobotController();

    // ========== 业务接口（供 UI 调用） ==========

    bool addRobot(int id, const QString &ip = "127.0.0.1");
    bool removeRobot(int id);
    bool updatePosition(int id, float x, float y);
    bool updateBattery(int id, int battery);
    bool updateRobotIp(int id, const QString &ip);
    bool updateStatus(int id, RobotStatus status);
    bool assignTask(int id, int taskId);
    bool finishTask(int id);
    bool cancelTask(int id);

    QString printAllRobots() const;
    QList<int> getOnlineRobots() const;
    bool isRobotExist(int id) const;

    // ===== 供主窗口列表展示 / 统计使用 =====
    const Robot *getRobot(int id) const;
    QList<int> getAllRobotIds() const;
    int getRobotCount() const;
    int getIdleCount() const;
    int getBusyCount() const;
    int getFaultCount() const;

signals:
    // ========== 通知 UI 更新的信号 ==========

    void logMessage(const QString &msg, int level);
    void robotAdded(int robotId);
    void robotRemoved(int robotId);
    void robotStatusChanged(int robotId, RobotStatus oldStatus, RobotStatus newStatus);
    void robotPositionChanged(int robotId, float x, float y);
    void robotBatteryChanged(int robotId, int battery);
    void robotTaskAssigned(int robotId, int taskId);
    void robotTaskFinished(int robotId, int taskId);
    void robotLowBattery(int robotId, int battery);
    void robotFaultDetected(int robotId, int faultCode);

private:
    RobotManager *m_manager;
};

#endif // ROBOTCONTROLLER_H