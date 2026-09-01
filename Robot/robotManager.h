#ifndef ROBOTMANAGER_H
#define ROBOTMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QMutex>
#include <QDebug>
#include "robot.h"

class RobotManager : public QObject
{
    Q_OBJECT

public:
    explicit RobotManager(QObject *parent = nullptr);
    ~RobotManager();

    // ========== 机器人管理 ==========

    bool addRobot(const Robot &robot);
    bool addRobot(int id, const QString &ip = QString());
    bool removeRobot(int id);
    void clearAll();

    Robot *getRobot(int id);
    const Robot *getRobot(int id) const;
    bool hasRobot(int id) const;
    int getRobotCount() const;

    QList<int> getAllRobotIds() const;
    QList<Robot> getAllRobots() const;

    // ========== 状态更新（每个方法独立判断变化）==========

    bool updateRobotPosition(int id, float x, float y);
    bool updateRobotBattery(int id, int battery);
    bool updateRobotSpeed(int id, float speed);
    bool updateRobotStatusEnum(int id, RobotStatus status);
    bool updateRobotTask(int id, int taskId);
    bool updateRobotIp(int id, const QString &ip);

    // 批量更新（调用上面的独立方法）
    bool updateRobotStatus(int id, float x, float y, int battery,
                           float speed, RobotStatus status);

    // ========== 状态查询 ==========

    QList<int> getRobotsByStatus(RobotStatus status) const;
    QList<int> getIdleRobots() const;
    QList<int> getOnlineRobots() const;
    QList<int> getFaultRobots() const;
    QList<int> getBusyRobots() const;
    QList<int> getLowBatteryRobots(int threshold = 20) const;
    QList<int> getRobotsSortedByLoad() const;

    // ========== 任务相关 ==========

    bool assignTaskToRobot(int robotId, int taskId);
    bool finishRobotTask(int robotId);
    bool cancelRobotTask(int robotId);
    int getRobotCurrentTask(int robotId) const;
    bool isRobotBusy(int robotId) const;

    // ========== 统计 ==========

    int getCountByStatus(RobotStatus status) const;
    int getIdleCount() const;
    int getBusyCount() const;
    int getFaultCount() const;
    int getOnlineCount() const;
    int getLowBatteryCount(int threshold = 20) const;

    // ========== 调试 ==========

    QString printAllRobots() const;
    QString printRobotBrief(int id) const;

signals:
    // 日志信号（锁内可直接发射，仅用于打印/记录）
    void logMessage(const QString &msg, int level);

    // 业务信号（必须在锁外发射，避免死锁）
    void robotAdded(int robotId);
    void robotRemoved(int robotId);
    void robotStatusChanged(int robotId, RobotStatus oldStatus, RobotStatus newStatus);
    void robotPositionChanged(int robotId, float x, float y);
    void robotBatteryChanged(int robotId, int battery);
    void robotSpeedChanged(int robotId, float speed);
    void robotIpChanged(int robotId, const QString &ip);
    void robotTaskAssigned(int robotId, int taskId);
    void robotTaskFinished(int robotId, int taskId);
    void robotTaskCancelled(int robotId, int taskId);
    void robotFaultDetected(int robotId, int faultCode);
    void robotLowBattery(int robotId, int battery);

private:
    QMap<int, Robot> m_robot;
    mutable QMutex m_mutex;
    int m_lowBatteryThreshold;
};

#endif // ROBOTMANAGER_H