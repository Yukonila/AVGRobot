#ifndef ROBOTCONTROLLER_H
#define ROBOTCONTROLLER_H

#include <QObject>
#include <QTimer>
#include "robotmanager.h"
#include "taskmanager.h"
#include "taskscheduler.h"

class RobotController : public QObject
{
    Q_OBJECT

public:
    explicit RobotController(QObject *parent = nullptr);
    ~RobotController();

    // ========== 机器人管理 ==========
    bool addRobot(int id, const QString &ip = "127.0.0.1");
    bool removeRobot(int id);
    bool updatePosition(int id, float x, float y);
    bool updateBattery(int id, int battery);
    bool updateSpeed(int id, float speed);
    bool updateStatus(int id, RobotStatus status);
    const Robot *getRobot(int id) const;
    Robot *getRobot(int id);
    QList<int> getAllRobotIds() const;
    int getRobotCount() const;
    int getIdleCount() const;
    int getBusyCount() const;
    int getFaultCount() const;
    bool isRobotBusy(int id) const;
    bool updateRobotIp(int id, const QString &ip);

    // ========== 任务管理 ==========
    bool addTask(int taskId, int priority, float startX, float startY,
                 float endX, float endY, const QString &desc = "");
    bool addTask(const Task &task);
    bool removeTask(int taskId);
    Task *getTask(int taskId);
    QList<int> getAllTaskIds() const;
    QList<int> getPendingTaskIds() const;
    QList<int> getExecutingTaskIds() const;
    int getPendingCount() const;
    int getExecutingCount() const;
    int getCompletedCount() const;
    int getFailedCount() const;
    int getCancelledCount() const;

    // ========== 调度控制 ==========
    void startScheduler(int intervalMs = 3000);
    void stopScheduler();
    void scheduleOnce();
    bool isSchedulerRunning() const;

    // ========== 数据持久化(DataManager) ==========
    bool saveData(const QString &filePath = "");
    bool loadData(const QString &filePath = "");

    // ========== 调试 ==========
    QString printAllRobots() const;
    QString printAllTasks() const;

signals:
    // 日志信号
    void logMessage(const QString &msg, int level);

    // 机器人信号（透传）
    void robotAdded(int robotId);
    void robotRemoved(int robotId);
    void robotStatusChanged(int robotId, RobotStatus oldStatus, RobotStatus newStatus);
    void robotPositionChanged(int robotId, float x, float y);
    void robotBatteryChanged(int robotId, int battery);

    // 任务信号（透传）
    void taskAdded(int taskId);
    void taskRemoved(int taskId);
    void taskAssigned(int taskId, int robotId);
    void taskFinished(int taskId);
    void taskFailed(int taskId);
    void taskCancelled(int taskId);

    // 调度信号
    void schedulerStarted();
    void schedulerStopped();
    void robotReturnedHome(int robotId);

private:
    RobotManager *m_robotManager;
    TaskManager *m_taskManager;
    TaskScheduler *m_scheduler;

    // 移动模拟（调度运行期间，让忙碌机器人逐步移向任务终点）
    QTimer *m_simTimer;
    void stepRobots();   // 每拍推进机器人位置
    int m_simIntervalMs;
};

#endif // ROBOTCONTROLLER_H