#ifndef ROBOTCONTROLLER_H
#define ROBOTCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QHash>
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
    void setEnableReturnHome(bool enable);   // 自动回原点开关(空闲即回)
    bool isReturnHomeEnabled() const;
    bool returnIdleRobotsToHome();           // 手动“全部(空闲)回原点”，不打断执行中任务
    void setSimulationMode(bool simulate);   // true=模拟移动; false=由TCP等真实数据更新位置
    bool isSimulationMode() const;

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

    // 移动模拟（调度运行期间，让机器人逐段移动：任务起点→任务终点→回原点）
    QTimer *m_simTimer;
    void stepRobots(); // 每拍推进机器人位置
    int m_simIntervalMs;
    bool m_isReturnHome;    // 自动回原点开关(默认开)
    bool m_homeIdleNow;     // “全部回原点”一次性触发标记
    bool m_simulateMovement;// 模拟模式=true; TCP接入后由真实数据更新位置=false

    // 每台机器人“当前任务→是否已到达过任务起点(锁定下一步去终点)”
    QHash<int, int> m_robotTaskPhase;   // robotId -> taskId
    QHash<int, bool> m_robotStartDone;  // robotId -> 已到过起点

    // 回原点阶段日志标记：0=未提示,1=已提示返回中,2=已提示回到
    QHash<int, int> m_robotReturnStage;
};

#endif // ROBOTCONTROLLER_H