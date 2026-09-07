#include "robotcontroller.h"
#include "Data/datamanager.h"
#include <QDebug>
#include <cmath>

RobotController::RobotController(QObject *parent)
    : QObject(parent), m_robotManager(new RobotManager(this)), m_taskManager(new TaskManager(this)), m_scheduler(nullptr), m_simTimer(nullptr), m_simIntervalMs(250)
{
    // ========== 连接 RobotManager 信号 ==========
    connect(m_robotManager, &RobotManager::logMessage,
            this, &RobotController::logMessage);
    connect(m_robotManager, &RobotManager::robotAdded,
            this, &RobotController::robotAdded);
    connect(m_robotManager, &RobotManager::robotRemoved,
            this, &RobotController::robotRemoved);
    connect(m_robotManager, &RobotManager::robotStatusChanged,
            this, &RobotController::robotStatusChanged);
    connect(m_robotManager, &RobotManager::robotPositionChanged,
            this, &RobotController::robotPositionChanged);
    connect(m_robotManager, &RobotManager::robotBatteryChanged,
            this, &RobotController::robotBatteryChanged);

    // ========== 连接 TaskManager 信号 ==========
    connect(m_taskManager, &TaskManager::logMessage,
            this, &RobotController::logMessage);
    connect(m_taskManager, &TaskManager::taskAdded,
            this, &RobotController::taskAdded);
    connect(m_taskManager, &TaskManager::taskRemoved,
            this, &RobotController::taskRemoved);
    // 注：taskAssigned / taskFinished 由调度器(TaskScheduler)统一转发，
    //     这里不再转发，避免与 TaskManager 各自触发导致信号重复。
    connect(m_taskManager, &TaskManager::taskFailed,
            this, &RobotController::taskFailed);
    connect(m_taskManager, &TaskManager::taskCancelled,
            this, &RobotController::taskCancelled);

    // ========== 创建调度器 ==========
    m_scheduler = new TaskScheduler(m_taskManager, m_robotManager, this);
    connect(m_scheduler, &TaskScheduler::logMessage,
            this, &RobotController::logMessage);
    connect(m_scheduler, &TaskScheduler::taskAssigned,
            this, &RobotController::taskAssigned);
    connect(m_scheduler, &TaskScheduler::taskCompleted,
            this, &RobotController::taskFinished);
    connect(m_scheduler, &TaskScheduler::robotReturnedHome,
            this, &RobotController::robotReturnedHome);

    // ========== 移动模拟定时器（只在调度运行时生效） ==========
    m_simTimer = new QTimer(this);
    m_simTimer->setInterval(m_simIntervalMs);
    connect(m_simTimer, &QTimer::timeout, this, [this]() { stepRobots(); });

    emit logMessage("[RobotController] 初始化完成", 0);
}

RobotController::~RobotController()
{
    if (m_scheduler)
    {
        m_scheduler->stop();
    }
}

// ========== 机器人管理 ==========

bool RobotController::addRobot(int id, const QString &ip)
{
    return m_robotManager->addRobot(id, ip);
}

bool RobotController::removeRobot(int id)
{
    return m_robotManager->removeRobot(id);
}

bool RobotController::updatePosition(int id, float x, float y)
{
    return m_robotManager->updateRobotPosition(id, x, y);
}

bool RobotController::updateBattery(int id, int battery)
{
    return m_robotManager->updateRobotBattery(id, battery);
}

bool RobotController::updateSpeed(int id, float speed)
{
    return m_robotManager->updateRobotSpeed(id, speed);
}

bool RobotController::updateStatus(int id, RobotStatus status)
{
    return m_robotManager->updateRobotStatus(id, status);
}

const Robot *RobotController::getRobot(int id) const
{
    return m_robotManager->getRobot(id);
}

Robot *RobotController::getRobot(int id)
{
    return m_robotManager->getRobot(id);
}

QList<int> RobotController::getAllRobotIds() const
{
    return m_robotManager->getAllRobotIds();
}

int RobotController::getRobotCount() const
{
    return m_robotManager->getRobotCount();
}

int RobotController::getIdleCount() const
{
    return m_robotManager->getIdleCount();
}

int RobotController::getBusyCount() const
{
    return m_robotManager->getBusyCount();
}

int RobotController::getFaultCount() const
{
    return m_robotManager->getFaultCount();
}

bool RobotController::isRobotBusy(int id) const
{
    return m_robotManager->isRobotBusy(id);
}

bool RobotController::updateRobotIp(int id, const QString &ip)
{
    return m_robotManager->updateRobotIp(id, ip);
}

// ========== 任务管理 ==========

bool RobotController::addTask(int taskId, int priority, float startX, float startY,
                              float endX, float endY, const QString &desc)
{
    Task task(taskId, priority, startX, startY, endX, endY, desc);
    return m_taskManager->addTask(task);
}

bool RobotController::addTask(const Task &task)
{
    return m_taskManager->addTask(task);
}

bool RobotController::removeTask(int taskId)
{
    return m_taskManager->removeTask(taskId);
}

Task *RobotController::getTask(int taskId)
{
    return m_taskManager->getTask(taskId);
}

QList<int> RobotController::getAllTaskIds() const
{
    return m_taskManager->getAllTaskIds();
}

QList<int> RobotController::getPendingTaskIds() const
{
    return m_taskManager->getPendingTaskIds();
}

QList<int> RobotController::getExecutingTaskIds() const
{
    return m_taskManager->getExecutingTaskIds();
}

int RobotController::getPendingCount() const
{
    return m_taskManager->getPendingCount();
}

int RobotController::getExecutingCount() const
{
    return m_taskManager->getExecutingCount();
}

int RobotController::getCompletedCount() const
{
    return m_taskManager->getCompletedCount();
}

int RobotController::getFailedCount() const
{
    return m_taskManager->getFailedCount();
}

int RobotController::getCancelledCount() const
{
    return m_taskManager->getCancelledCount();
}

// ========== 调度控制 ==========

void RobotController::startScheduler(int intervalMs)
{
    if (m_scheduler)
    {
        m_scheduler->start(intervalMs);
        emit schedulerStarted();
    }
    if (m_simTimer && !m_simTimer->isActive())
        m_simTimer->start();
}

void RobotController::stopScheduler()
{
    if (m_scheduler)
    {
        m_scheduler->stop();
        emit schedulerStopped();
    }
    if (m_simTimer)
        m_simTimer->stop();
}

void RobotController::scheduleOnce()
{
    if (m_scheduler)
    {
        m_scheduler->scheduleOnce();
    }
}

bool RobotController::isSchedulerRunning() const
{
    return m_scheduler ? m_scheduler->isRunning() : false;
}

// ========== 移动模拟（让任务能演示完成） ==========

void RobotController::stepRobots()
{
    if (!m_simTimer || !m_simTimer->isActive())
        return;

    const float dt = m_simIntervalMs / 1000.0f;
    const float defaultSpeed = 8.0f; // 无速度配置时的模拟速度(单位/秒)

    const QList<int> ids = m_robotManager->getAllRobotIds();
    for (int id : ids)
    {
        Robot *r = m_robotManager->getRobot(id);
        if (!r)
            continue;

        // 只移动"执行任务中(忙碌)"的机器人
        if (r->getStatus() != RobotStatus::Busy)
            continue;

        const int taskId = r->getTask();
        if (taskId < 0)
            continue;

        const Task *task = m_taskManager->getTask(taskId);
        if (!task)
            continue;

        const float tx = task->getEndX();
        const float ty = task->getEndY();
        const float px = r->getPx();
        const float py = r->getPy();
        const float dx = tx - px;
        const float dy = ty - py;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 1e-4f)
            continue; // 已到终点，等待调度器判定完成

        const float speed = r->getSpeed() > 0.0f ? r->getSpeed() : defaultSpeed;
        const float step = speed * dt;
        const float move = step > dist ? dist : step; // 不越过终点

        m_robotManager->updateRobotPosition(id,
                                            px + dx / dist * move,
                                            py + dy / dist * move);
    }
}

// ========== 数据持久化 ==========

bool RobotController::saveData(const QString &filePath)
{
    Q_UNUSED(filePath); // 使用 DataManager 默认路径
    DataManager dm(this);
    connect(&dm, &DataManager::logMessage, this, &RobotController::logMessage);

    // 汇总当前机器人与任务
    QList<Robot> robots = m_robotManager->getAllRobots();
    QList<Task> tasks;
    const QList<int> ids = m_taskManager->getAllTaskIds();
    for (int id : ids)
    {
        const Task *t = m_taskManager->getTask(id);
        if (t)
            tasks.append(*t);
    }

    int interval = m_scheduler ? m_scheduler->getInterval() : 3000;
    return dm.saveAllData(robots, tasks, 8888, interval);
}

bool RobotController::loadData(const QString &filePath)
{
    Q_UNUSED(filePath); // 使用 DataManager 默认路径
    DataManager dm(this);
    connect(&dm, &DataManager::logMessage, this, &RobotController::logMessage);

    QList<Robot> robots;
    QList<Task> tasks;
    int port = 8888;
    int interval = 3000;
    if (!dm.loadAllData(robots, tasks, port, interval))
    {
        emit logMessage("[RobotController] 载入数据失败", 2);
        return false;
    }

    // 用载入数据替换当前数据
    bool wasRunning = m_scheduler && m_scheduler->isRunning();
    if (m_scheduler)
        m_scheduler->stop();

    m_robotManager->clearAll();
    m_taskManager->clearAll();

    for (const Robot &r : robots)
    {
        if (!m_robotManager->addRobot(r))
            emit logMessage("[RobotController] 载入机器人 " + QString::number(r.getId()) + " 失败", 2);
    }
    for (const Task &t : tasks)
    {
        if (!m_taskManager->addTask(t))
            emit logMessage("[RobotController] 载入任务 " + QString::number(t.getTaskId()) + " 失败", 2);
    }

    if (wasRunning && m_scheduler)
        m_scheduler->start(interval);

    emit logMessage("[RobotController] 数据载入完成: " +
                        QString::number(robots.size()) + " 个机器人, " +
                        QString::number(tasks.size()) + " 个任务",
                    0);
    return true;
}

// ========== 调试 ==========

QString RobotController::printAllRobots() const
{
    return m_robotManager->printAllRobots();
}

QString RobotController::printAllTasks() const
{
    return m_taskManager->printAllTasks();
}