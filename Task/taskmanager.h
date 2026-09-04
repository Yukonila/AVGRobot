#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QMutex>
#include "task.h"

class TaskManager : public QObject
{
    Q_OBJECT
public:
    explicit TaskManager(QObject *parent = nullptr);

    // ========== 任务生命周期 ==========
    bool addTask(const Task &task);
    bool removeTask(int taskId);
    void clearAll();

    // ========== 查询 ==========
    Task *getTask(int taskId);
    const Task *getTask(int taskId) const;
    bool hasTask(int taskId) const;
    int getTaskCount() const;
    QList<int> getAllTaskIds() const;

    // ========== 按状态查询 ==========
    QList<int> getTaskIdsByStatus(TaskStatus status) const;
    QList<int> getPendingTaskIds() const;
    QList<int> getExecutingTaskIds() const;
    QList<int> getCompletedTaskIds() const;
    QList<int> getFailedTaskIds() const;
    QList<int> getCancelledTaskIds() const;
    QList<Task> getPendingTasks() const;
    QList<Task> getExecutingTasks() const;
    QList<Task> getCompletedTasks() const;
    QList<Task> getFailedTasks() const;
    QList<Task> getCancelledTasks() const;

    // ========== 统计 ==========
    int getCountByStatus(TaskStatus status) const;
    int getPendingCount() const;
    int getExecutingCount() const;
    int getCompletedCount() const;
    int getFailedCount() const;
    int getCancelledCount() const;

    // ========== 状态变更 ==========
    bool assignTask(int taskId, int robotId); // Pending → Executing
    bool finishTask(int taskId);              // Executing → Completed
    bool failTask(int taskId);                // 任意 → Failed
    bool cancelTask(int taskId);              // 任意 → Cancelled
    bool reassignTask(int taskId);            // 重新放回待分配队列

    // ========== 调度器专用 ==========
    int getNextPendingTask(); // 获取下一个待分配任务

    // ========== 调试 ==========
    QString printAllTasks() const;
    QString printTasksByStatus(TaskStatus status) const;

signals:
    void taskAdded(int taskId);
    void taskRemoved(int taskId);
    void taskAssigned(int taskId, int robotId);
    void taskFinished(int taskId);
    void taskFailed(int taskId);
    void taskCancelled(int taskId);
    void taskReassigned(int taskId);

private:
    void addToPendingList(int taskId);
    void removeFromPendingList(int taskId);
    void reorderPendingList();
    void moveToStatusList(int taskId, TaskStatus newStatus);
    void removeFromAllStatusLists(int taskId);

    QMap<int, Task> m_allTasks;    // 所有任务 (taskId → Task)
    QList<int> m_pendingList;      // 待分配任务ID列表（按优先级排序）
    QMap<int, int> m_executingMap; // 执行中 (taskId → robotId)
    QList<int> m_completedList;    // 已完成
    QList<int> m_failedList;       // 异常
    QList<int> m_cancelledList;    // 已取消
    mutable QMutex m_mutex;
};

#endif // TASKMANAGER_H
