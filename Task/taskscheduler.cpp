#include "taskscheduler.h"
#include <cmath>
#include <algorithm>

// ========== 构造/析构 ==========

TaskScheduler::TaskScheduler(TaskManager *taskManager,
                             RobotManager *robotManager,
                             QObject *parent)
    : QObject(parent), m_taskManager(taskManager), m_robotManager(robotManager), m_timer(nullptr), m_intervalMs(3000), m_isRunning(false)
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

    // 2. 检查执行中的任务进度
    checkExecutingTasks();

    // 3. 让完成任务的机器人回原点
    returnRobotsToHome();
}

void TaskScheduler::assignPendingTasks()
{
    // 获取下一个待分配任务
    int taskId = m_taskManager->getNextPendingTask();
    if (taskId == -1)
    {
        // 没有待分配任务，不重复发信号
        return;
    }

    // 获取空闲机器人
    QList<int> idleRobots = m_robotManager->getIdleRobots();
    if (idleRobots.isEmpty())
    {
        emit logMessage("[TaskScheduler] 没有空闲机器人，任务 " +
                            QString::number(taskId) + " 等待中",
                        1);
        emit noAvailableRobot(taskId);
        return;
    }

    // 选择最优机器人
    int bestRobot = selectBestRobotForTask(taskId);
    if (bestRobot == -1)
    {
        emit logMessage("[TaskScheduler] 无法为任务 " +
                            QString::number(taskId) + " 找到合适的机器人",
                        2);
        return;
    }

    // 分配任务
    if (m_taskManager->assignTask(taskId, bestRobot))
    {
        emit taskAssigned(taskId, bestRobot);
        emit logMessage("[TaskScheduler] 任务 " + QString::number(taskId) +
                            " 已分配给机器人 " + QString::number(bestRobot),
                        0);

        // 同步机器人侧：记录正在执行的任务ID 并置为忙碌
        // （机器人表的"任务ID"列、移除保护、isRobotBusy 统计随之一致）
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

        // 检查机器人是否到达终点
        // 实际项目中由 TCP 上报位置，这里用模拟方式
        float dx = robot->getPx() - task->getEndX();
        float dy = robot->getPy() - task->getEndY();
        float distance = calculateDistance(robot->getPx(), robot->getPy(),
                                           task->getEndX(), task->getEndY());

        // 如果距离终点小于 0.5 单位，认为到达
        if (distance < 0.5f)
        {
            if (m_taskManager->finishTask(taskId))
            {
                emit taskCompleted(taskId, robotId);
                emit logMessage("[TaskScheduler] 任务 " + QString::number(taskId) +
                                    " 已完成 (机器人 " + QString::number(robotId) + ")",
                                0);

                // 同步机器人侧：清除任务ID 并回到空闲，准备接收下一任务
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

        // 检查机器人是否空闲且不在原点
        if (robot->getStatus() != RobotStatus::Idle)
            continue;

        float distance = calculateDistance(robot->getPx(), robot->getPy(), 0.0f, 0.0f);

        // 如果已经回到原点
        if (distance < 0.1f)
        {
            if (robot->getPx() != 0.0f || robot->getPy() != 0.0f)
            {
                robot->setPx(0.0f);
                robot->setPy(0.0f);
            }
            continue;
        }

        // 模拟向原点移动（实际由 TCP 控制）
        // 简化：直接回到原点
        // 实际项目中，应该下发路径给机器人，这里只是演示
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
