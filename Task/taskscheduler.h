#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QMutex>
#include "taskmanager.h"
#include "robotmanager.h"

class TaskScheduler : public QObject
{
    Q_OBJECT

public:
    explicit TaskScheduler(TaskManager *taskManager,
                           RobotManager *robotManager,
                           QObject *parent = nullptr);
    ~TaskScheduler();

    // ========== 调度控制 ==========
    void start(int intervalMs = 3000);
    void stop();
    bool isRunning() const;
    void scheduleOnce(); // 手动触发一次调度

    // ========== 配置 ==========
    void setInterval(int ms);
    int getInterval() const;

signals:
    // 调度事件信号
    void taskAssigned(int taskId, int robotId);
    void taskCompleted(int taskId, int robotId);
    void robotReturnedHome(int robotId);
    void noAvailableRobot(int taskId);
    void noPendingTask();

    // 日志信号
    void logMessage(const QString &msg, int level);

private slots:
    void onTimerTimeout();

private:
    // ========== 调度核心方法 ==========
    void doScheduling();
    void assignPendingTasks();
    void checkExecutingTasks();
    void returnRobotsToHome();

    // ========== 调度算法（文档6.1节） ==========
    int selectBestRobotForTask(int taskId);
    QList<int> filterCandidates(int taskId);
    void sortByLoad(QList<int> &robotIds);
    void sortByDistance(QList<int> &robotIds, float targetX, float targetY);

    // ========== 辅助方法 ==========
    float calculateDistance(float x1, float y1, float x2, float y2) const;

    TaskManager *m_taskManager;
    RobotManager *m_robotManager;
    QTimer *m_timer;
    int m_intervalMs;
    bool m_isRunning;
};

#endif // TASKSCHEDULER_H