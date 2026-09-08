#include "taskmanager.h"

TaskManager::TaskManager(QObject *parent)
    : QObject{parent}
{
}

TaskManager::~TaskManager()
{
    clearAll();
}

bool TaskManager::addTask(const Task &task)
{
    int taskId = task.getTaskId();
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        if (taskId < 0)
        {
            emit logMessage("[TaskManager] 无效的任务ID: " + QString::number(taskId), 2);
            return false;
        }

        if (m_allTasks.contains(taskId))
        {
            emit logMessage("[TaskManager] 任务ID " + QString::number(taskId) + " 已存在", 2);
            return false;
        }

        m_allTasks[taskId] = task;

        if (task.getStatus() == TaskStatus::Pending)
        {
            addToPendingList(taskId);
        }
        else if (task.getStatus() == TaskStatus::Executing)
        {
            m_executingMap[taskId] = task.getAssignedRobotId();
        }
        else if (task.getStatus() == TaskStatus::Completed)
        {
            m_completedList.append(taskId);
        }
        else if (task.getStatus() == TaskStatus::Failed)
        {
            m_failedList.append(taskId);
        }
        else if (task.getStatus() == TaskStatus::Cancelled)
        {
            m_cancelledList.append(taskId);
        }

        success = true;
        emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 已添加", 0);
    }
    if (success)
    {
        emit taskAdded(taskId);
    }

    return success;
}

bool TaskManager::removeTask(int taskId)
{
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_allTasks.contains(taskId))
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 不存在", 2);
            return false;
        }

        removeFromAllStatusLists(taskId);
        m_allTasks.remove(taskId);

        success = true;
        emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 已移除", 0);
    }

    if (success)
    {
        emit taskRemoved(taskId);
    }

    return success;
}

void TaskManager::clearAll()
{
    QList<int> ids;

    {
        QMutexLocker locker(&m_mutex);

        ids = m_allTasks.keys();
        m_allTasks.clear();
        m_pendingList.clear();
        m_executingMap.clear();
        m_completedList.clear();
        m_failedList.clear();
        m_cancelledList.clear();

        emit logMessage("[TaskManager] 所有任务已清空，共移除 " + QString::number(ids.size()) + " 个任务", 0);
    }

    for (int id : ids)
    {
        emit taskRemoved(id);
    }
}

Task *TaskManager::getTask(int taskId)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_allTasks.find(taskId);
    if (it == m_allTasks.end())
    {
        return nullptr;
    }
    return &it.value();
}

const Task *TaskManager::getTask(int taskId) const
{
    QMutexLocker locker(&m_mutex);

    auto it = m_allTasks.find(taskId);
    if (it == m_allTasks.end())
    {
        return nullptr;
    }
    return &it.value();
}

bool TaskManager::hasTask(int taskId) const
{
    QMutexLocker locker(&m_mutex);

    return m_allTasks.contains(taskId);
}

int TaskManager::getTaskCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_allTasks.count();
}

QList<int> TaskManager::getAllTaskIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_allTasks.keys();
}

QList<int> TaskManager::getTaskIdsByStatus(TaskStatus status) const
{
    QMutexLocker locker(&m_mutex);

    switch (status)
    {
    case TaskStatus::Pending:
        return m_pendingList;
    case TaskStatus::Executing:
        return m_executingMap.keys();
    case TaskStatus::Completed:
        return m_completedList;
    case TaskStatus::Failed:
        return m_failedList;
    case TaskStatus::Cancelled:
        return m_cancelledList;
    default:
        return QList<int>();
    }
}

QList<int> TaskManager::getPendingTaskIds() const
{
    return getTaskIdsByStatus(TaskStatus::Pending);
}

QList<int> TaskManager::getExecutingTaskIds() const
{
    return getTaskIdsByStatus(TaskStatus::Executing);
}

QList<int> TaskManager::getCompletedTaskIds() const
{
    return getTaskIdsByStatus(TaskStatus::Completed);
}

QList<int> TaskManager::getFailedTaskIds() const
{
    return getTaskIdsByStatus(TaskStatus::Failed);
}

QList<int> TaskManager::getCancelledTaskIds() const
{
    return getTaskIdsByStatus(TaskStatus::Cancelled);
}

QList<Task> TaskManager::getPendingTasks() const
{
    QMutexLocker locker(&m_mutex);
    QList<Task> result;

    for (int id : m_pendingList)
    {
        if (m_allTasks.contains(id))
        {
            result.append(m_allTasks[id]);
        }
    }
    return result;
}

QList<Task> TaskManager::getExecutingTasks() const
{
    QMutexLocker locker(&m_mutex);
    QList<Task> result;

    for (int id : m_executingMap.keys())
    {
        if (m_allTasks.contains(id))
        {
            result.append(m_allTasks[id]);
        }
    }
    return result;
}

QList<Task> TaskManager::getCompletedTasks() const
{
    QMutexLocker locker(&m_mutex);
    QList<Task> result;

    for (int id : m_completedList)
    {
        if (m_allTasks.contains(id))
        {
            result.append(m_allTasks[id]);
        }
    }
    return result;
}

QList<Task> TaskManager::getFailedTasks() const
{
    QMutexLocker locker(&m_mutex);
    QList<Task> result;

    for (int id : m_failedList)
    {
        if (m_allTasks.contains(id))
        {
            result.append(m_allTasks[id]);
        }
    }
    return result;
}

QList<Task> TaskManager::getCancelledTasks() const
{
    QMutexLocker locker(&m_mutex);
    QList<Task> result;

    for (int id : m_cancelledList)
    {
        if (m_allTasks.contains(id))
        {
            result.append(m_allTasks[id]);
        }
    }
    return result;
}

int TaskManager::getCountByStatus(TaskStatus status) const
{
    return getTaskIdsByStatus(status).size();
}

int TaskManager::getPendingCount() const
{
    return getCountByStatus(TaskStatus::Pending);
}

int TaskManager::getExecutingCount() const
{
    return getCountByStatus(TaskStatus::Executing);
}

int TaskManager::getCompletedCount() const
{
    return getCountByStatus(TaskStatus::Completed);
}

int TaskManager::getFailedCount() const
{
    return getCountByStatus(TaskStatus::Failed);
}

int TaskManager::getCancelledCount() const
{
    return getCountByStatus(TaskStatus::Cancelled);
}

bool TaskManager::assignTask(int taskId, int robotId)
{
    bool success = false;
    {
        QMutexLocker locker(&m_mutex);

        if (!m_allTasks.contains(taskId))
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 不存在", 2);
            return false;
        }

        Task &task = m_allTasks[taskId];

        if (task.getStatus() != TaskStatus::Pending)
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) +
                                " 不是待分配状态，当前状态: " + task.getStatusString(),
                            2);
            return false;
        }

        removeFromAllStatusLists(taskId);
        task.markAsExecting();
        task.setAssignedRobotId(robotId);
        m_executingMap[taskId] = robotId;
        success = true;

        emit logMessage("[TaskManager] 任务 " + QString::number(taskId) +
                            " 已分配给机器人 " + QString::number(robotId),
                        0);
    }

    if (success)
    {
        emit taskAssigned(taskId, robotId);
    }

    return success;
}

bool TaskManager::finishTask(int taskId)
{
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_allTasks.contains(taskId))
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 不存在", 2);
            return false;
        }

        Task &task = m_allTasks[taskId];

        if (task.getStatus() != TaskStatus::Executing)
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) +
                                " 不是执行中状态，当前状态: " + task.getStatusString(),
                            2);
            return false;
        }

        m_executingMap.remove(taskId);
        task.markAsCompleted();
        m_completedList.append(taskId);

        success = true;
        emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 已完成", 0);
    }

    if (success)
    {
        emit taskFinished(taskId);
    }

    return success;
}

bool TaskManager::failTask(int taskId)
{
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_allTasks.contains(taskId))
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 不存在", 2);
            return false;
        }

        Task &task = m_allTasks[taskId];

        if (task.isFinished())
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) +
                                " 已经是终态: " + task.getStatusString(),
                            2);
            return false;
        }

        removeFromAllStatusLists(taskId);
        task.markAsFailed();
        m_failedList.append(taskId);

        success = true;
        emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 标记为异常", 2);
    }

    if (success)
    {
        emit taskFailed(taskId);
    }

    return success;
}

bool TaskManager::cancelTask(int taskId)
{
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_allTasks.contains(taskId))
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 不存在", 2);
            return false;
        }

        Task &task = m_allTasks[taskId];

        if (task.isFinished())
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) +
                                " 已经是终态: " + task.getStatusString(),
                            2);
            return false;
        }

        removeFromAllStatusLists(taskId);
        task.markAsCancelled();
        m_cancelledList.append(taskId);

        success = true;
        emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 已取消", 0);
    }

    if (success)
    {
        emit taskCancelled(taskId);
    }

    return success;
}

bool TaskManager::reassignTask(int taskId)
{
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        if (!m_allTasks.contains(taskId))
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 不存在", 2);
            return false;
        }

        Task &task = m_allTasks[taskId];

        if (task.isFinished())
        {
            emit logMessage("[TaskManager] 任务 " + QString::number(taskId) +
                                " 已经是终态，不能重新分配: " + task.getStatusString(),
                            2);
            return false;
        }

        removeFromAllStatusLists(taskId);
        task.markAsPending();
        task.setAssignedRobotId(-1);
        addToPendingList(taskId);

        success = true;
        emit logMessage("[TaskManager] 任务 " + QString::number(taskId) + " 已重新放入待分配队列", 0);
    }

    if (success)
    {
        emit taskReassigned(taskId);
    }

    return success;
}

int TaskManager::getNextPendingTask()
{
    QMutexLocker locker(&m_mutex);

    if (m_pendingList.isEmpty())
    {
        return -1;
    }

    return m_pendingList.first();
}

void TaskManager::addToPendingList(int taskId)
{
    if (!m_pendingList.contains(taskId))
    {
        m_pendingList.append(taskId);
        reorderPendingList();
    }
}

void TaskManager::removeFromPendingList(int taskId)
{
    m_pendingList.removeAll(taskId);
}

void TaskManager::reorderPendingList()
{
    // 注意：本函数只在已持有 m_mutex 时被调用(addToPendingList)，
    //       因此排序比较器必须直接读 m_allTasks，不能再调用会加锁的 getTask()，
    //       否则非递归 QMutex 同线程二次加锁会自死锁。
    std::sort(m_pendingList.begin(), m_pendingList.end(),
              [this](int a, int b)
              {
                  auto ita = m_allTasks.find(a);
                  auto itb = m_allTasks.find(b);
                  if (ita == m_allTasks.end() || itb == m_allTasks.end())
                      return a < b;

                  if (ita->getPriority() != itb->getPriority())
                  {
                      return ita->getPriority() > itb->getPriority(); // 高优先级在前
                  }
                  return ita->getCreateTime() < itb->getCreateTime(); // 早创建在前
              });
}

void TaskManager::moveToStatusList(int taskId, TaskStatus newStatus)
{
    removeFromAllStatusLists(taskId);

    switch (newStatus)
    {
    case TaskStatus::Pending:
        addToPendingList(taskId);
        break;
    case TaskStatus::Executing:
        if (m_allTasks.contains(taskId))
        {
            m_executingMap[taskId] = m_allTasks[taskId].getAssignedRobotId();
        }
        break;
    case TaskStatus::Completed:
        m_completedList.append(taskId);
        break;
    case TaskStatus::Failed:
        m_failedList.append(taskId);
        break;
    case TaskStatus::Cancelled:
        m_cancelledList.append(taskId);
        break;
    default:
        break;
    }
}

void TaskManager::removeFromAllStatusLists(int taskId)
{
    m_pendingList.removeAll(taskId);
    m_executingMap.remove(taskId);
    m_completedList.removeAll(taskId);
    m_failedList.removeAll(taskId);
    m_cancelledList.removeAll(taskId);
}

// ========== 调试 ==========

QString TaskManager::printAllTasks() const
{
    QMutexLocker locker(&m_mutex);

    if (m_allTasks.isEmpty())
    {
        return "=== 没有任务 ===";
    }

    QString result;
    result += "=== 任务列表 (总数: " + QString::number(m_allTasks.size()) + ") ===\n";
    result += "待分配: " + QString::number(m_pendingList.size()) + " | ";
    result += "执行中: " + QString::number(m_executingMap.size()) + " | ";
    result += "已完成: " + QString::number(m_completedList.size()) + " | ";
    result += "异常: " + QString::number(m_failedList.size()) + " | ";
    result += "已取消: " + QString::number(m_cancelledList.size()) + "\n\n";

    for (auto it = m_allTasks.begin(); it != m_allTasks.end(); ++it)
    {
        result += it.value().toString() + "\n";
    }

    return result;
}

QString TaskManager::printTasksByStatus(TaskStatus status) const
{
    QMutexLocker locker(&m_mutex);

    QList<int> ids;
    switch (status)
    {
    case TaskStatus::Pending:
        ids = m_pendingList;
        break;
    case TaskStatus::Executing:
        ids = m_executingMap.keys();
        break;
    case TaskStatus::Completed:
        ids = m_completedList;
        break;
    case TaskStatus::Failed:
        ids = m_failedList;
        break;
    case TaskStatus::Cancelled:
        ids = m_cancelledList;
        break;
    default:
        break;
    }

    if (ids.isEmpty())
    {
        return "没有 " + QString::fromUtf8(Task().getStatusString().toUtf8()) + " 状态的任务";
    }

    QString result;
    result += "=== " + QString::fromUtf8(Task().getStatusString().toUtf8()) + " 任务 ===\n";
    for (int id : ids)
    {
        if (m_allTasks.contains(id))
        {
            result += m_allTasks[id].toBriefString() + "\n";
        }
    }
    return result;
}