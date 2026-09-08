#include "taskscheduler.h"
#include <cmath>
#include <algorithm>

// ========== 构造/析构 ==========

TaskScheduler::TaskScheduler(TaskManager *taskManager,
                             RobotManager *robotManager,
                             QObject *parent)
    : QObject(parent), m_taskManager(taskManager), m_robotManager(robotManager), m_timer(nullptr), m_intervalMs(3000), m_isRunning(false), m_isReturnHome(false)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TaskScheduler::onTimerTimeout);
}

TaskScheduler::~TaskScheduler()
{
    stop();
}

void TaskScheduler::start(int intervalMs)
{
    if (m_isRunning)
    {
        emit logMessage("[TaskScheduler] 调度引擎已在运行", 1);
        return;
    }
    m_intervalMs = intervalMs;
    m_timer->start(m_intervalMs);
    m_isRunning = true;

    emit logMessage("[TaskScheduler] 调度引擎已启动，间隔: " +
                        QString::number(m_intervalMs) + "ms",
                    0);
}

void TaskScheduler::stop()
{
    if (!m_isRunning)
    {
        return;
    }

    m_timer->stop();
    m_isRunning = false;
    emit logMessage("[TaskScheduler] 调度引擎已停止", 0);
}

bool TaskScheduler::isRunning() const
{
    return m_isRunning;
}

void TaskScheduler::scheduleOnce()
{
    emit logMessage("[TaskScheduler] 手动触发调度", 1);
    doScheduling();
}

void TaskScheduler::setEnableReturnHome(bool enable)
{
    m_isReturnHome = enable;
    emit logMessage("[TaskScheduler] 回原点功能 " +
                        QString(enable ? "已启用" : "已禁用"),
                    0);
}

bool TaskScheduler::isReturnHomeEnabled() const
{
    return m_isReturnHome;
}

void TaskScheduler::setInterval(int ms)
{
    if (ms < 100)
        ms = 100;
    m_intervalMs = ms;
    if (m_isRunning)
    {
        m_timer->setInterval(m_intervalMs);
    }
    emit logMessage("[TaskScheduler] 调度间隔已更新为: " + QString::number(m_intervalMs) + "ms", 1);
}

int TaskScheduler::getInterval() const
{
    return m_intervalMs;
}

// ========== 定时器槽 ==========
void TaskScheduler::onTimerTimeout()
{
    doScheduling();
}

// ========== 调度核心 ==========

void TaskScheduler::doScheduling()
{
    // 1. 分配待处理任务
    assignPendingTasks();

    // 2. 检查执行中的任务进度(到终点→完成)
    checkExecutingTasks();
}

void TaskScheduler::assignPendingTasks()
{
    // 获取下一个待分配任务
    int taskId = m_taskManager->getNextPendingTask();
    if (taskId == -1)
    {
        return;
    }

    // 获取空闲机器人
    QList<int> idleRobots = m_robotManager->getIdleRobots();
    int bestRobot = -1;

    if (idleRobots.isEmpty())
    {
        // —— 抢占：无空闲时，若待分配任务优先级高于某执行中任务，则抢占该机器人 ——
        const Task *pt = m_taskManager->getTask(taskId);
        int newPri = pt ? pt->getPriority() : -1;
        for (int rid : m_robotManager->getAllRobotIds())
        {
            const Robot *rb = m_robotManager->getRobot(rid);
            if (!rb || rb->getStatus() != RobotStatus::Busy)
                continue;
            int curTask = rb->getTask();
            if (curTask < 0)
                continue;
            const Task *cur = m_taskManager->getTask(curTask);
            if (!cur || cur->getPriority() >= newPri)
                continue;
            // 抢占：释放该机器人，原任务退回待分配队列
            m_robotManager->finishRobotTask(rid);
            m_taskManager->reassignTask(curTask);
            emit logMessage("[TaskScheduler] 高优先级任务 " + QString::number(taskId) +
                                " 抢占机器人 " + QString::number(rid) + " (原任务 " +
                                QString::number(curTask) + " 退回队列)",
                            3);
            bestRobot = rid;
            break;
        }
        if (bestRobot < 0)
        {
            // 只在任务首次开始等待时提示一次，避免每拍刷屏
            if (taskId != m_lastWaitingTask)
            {
                m_lastWaitingTask = taskId;
                emit logMessage("[TaskScheduler] 没有空闲/可抢占机器人，任务 " +
                                    QString::number(taskId) + " 待命中",
                                1);
                emit noAvailableRobot(taskId);
            }
            return;
        }
    }
    else
    {
        m_lastWaitingTask = -1;
        bestRobot = selectBestRobotForTask(taskId);
        if (bestRobot == -1)
        {
            emit logMessage("[TaskScheduler] 无法为任务 " +
                                QString::number(taskId) + " 找到合适的机器人",
                            2);
            return;
        }
    }

    // 分配任务
    if (m_taskManager->assignTask(taskId, bestRobot))
    {
        emit taskAssigned(taskId, bestRobot);
        emit logMessage("[TaskScheduler] 任务 " + QString::number(taskId) +
                            " 已分配给机器人 " + QString::number(bestRobot),
                        0);

        if (!m_robotManager->assignTaskToRobot(bestRobot, taskId))
        {
            emit logMessage("[TaskScheduler] 同步机器人 " + QString::number(bestRobot) +
                                " 任务状态失败",
                            2);
        }
    }
}

void TaskScheduler::checkExecutingTasks()
{
    QList<int> executingTasks = m_taskManager->getExecutingTaskIds();

    for (int taskId : executingTasks)
    {
        Task *task = m_taskManager->getTask(taskId);
        if (!task)
            continue;

        int robotId = task->getAssignedRobotId();
        const Robot *robot = m_robotManager->getRobot(robotId);
        if (!robot)
        {
            // 机器人不存在，任务失败
            m_taskManager->failTask(taskId);
            emit logMessage("[TaskScheduler] 任务 " + QString::number(taskId) +
                                " 失败: 机器人 " + QString::number(robotId) + " 不存在",
                            2);
            continue;
        }

        float dx = robot->getPx() - task->getEndX();
        float dy = robot->getPy() - task->getEndY();
        float distance = calculateDistance(robot->getPx(), robot->getPy(),
                                           task->getEndX(), task->getEndY());

        if (distance < 0.5f)
        {
            if (m_taskManager->finishTask(taskId))
            {
                emit taskCompleted(taskId, robotId);
                emit logMessage("[TaskScheduler] 任务 " + QString::number(taskId) +
                                    " 已完成 (机器人 " + QString::number(robotId) + ")",
                                0);

                if (!m_robotManager->finishRobotTask(robotId))
                {
                    emit logMessage("[TaskScheduler] 同步机器人 " + QString::number(robotId) +
                                        " 回到空闲失败",
                                    2);
                }
            }
        }
    }
}

void TaskScheduler::returnRobotsToHome()
{
    QList<int> allRobots = m_robotManager->getAllRobotIds();

    for (int robotId : allRobots)
    {
        Robot *robot = m_robotManager->getRobot(robotId);
        if (!robot)
            continue;

        if (robot->getStatus() != RobotStatus::Idle)
            continue;

        float distance = calculateDistance(robot->getPx(), robot->getPy(), 0.0f, 0.0f);

        if (distance < 0.1f)
        {
            if (robot->getPx() != 0.0f || robot->getPy() != 0.0f)
            {
                robot->setPx(0.0f);
                robot->setPy(0.0f);
            }
            continue;
        }

        robot->setPx(0.0f);
        robot->setPy(0.0f);
        emit robotReturnedHome(robotId);
        emit logMessage("[TaskScheduler] 机器人 " + QString::number(robotId) +
                            " 已回到原点",
                        0);
    }
}

// ========== 调度算法 ==========

int TaskScheduler::selectBestRobotForTask(int taskId)
{
    Task *task = m_taskManager->getTask(taskId);
    if (!task)
        return -1;

    // 第一步：筛选候选机器人（空闲机器人）
    QList<int> candidates = filterCandidates(taskId);
    if (candidates.isEmpty())
        return -1;

    // 第二步：按负载排序（最小负载优先）
    sortByLoad(candidates);

    // 第三步：按距离排序（最短距离优先）
    sortByDistance(candidates, task->getStartX(), task->getStartY());

    // 返回最优机器人
    return candidates.first();
}

QList<int> TaskScheduler::filterCandidates(int taskId)
{
    Q_UNUSED(taskId);
    // 获取所有空闲机器人
    return m_robotManager->getIdleRobots();
}

void TaskScheduler::sortByLoad(QList<int> &robotIds)
{
    std::sort(robotIds.begin(), robotIds.end(),
              [this](int a, int b)
              {
                  const Robot *ra = m_robotManager->getRobot(a);
                  const Robot *rb = m_robotManager->getRobot(b);
                  if (!ra || !rb)
                      return false;

                  // 按任务数排序：任务少的优先
                  int loadA = ra->getTask() != -1 ? 1 : 0;
                  int loadB = rb->getTask() != -1 ? 1 : 0;
                  return loadA < loadB;
              });
}

void TaskScheduler::sortByDistance(QList<int> &robotIds, float targetX, float targetY)
{
    std::stable_sort(robotIds.begin(), robotIds.end(),
                     [this, targetX, targetY](int a, int b)
                     {
                         const Robot *ra = m_robotManager->getRobot(a);
                         const Robot *rb = m_robotManager->getRobot(b);
                         if (!ra || !rb)
                             return false;

                         float distA = calculateDistance(ra->getPx(), ra->getPy(), targetX, targetY);
                         float distB = calculateDistance(rb->getPx(), rb->getPy(), targetX, targetY);
                         return distA < distB;
                     });
}

// ========== 辅助方法 ==========

float TaskScheduler::calculateDistance(float x1, float y1, float x2, float y2) const
{
    float dx = x1 - x2;
    float dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}
