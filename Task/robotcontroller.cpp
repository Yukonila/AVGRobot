#include "robotcontroller.h"
#include "Data/datamanager.h"
#include <QDebug>
#include <cmath>

RobotController::RobotController(QObject *parent)
    : QObject(parent), m_robotManager(new RobotManager(this)), m_taskManager(new TaskManager(this)), m_scheduler(nullptr), m_simTimer(nullptr), m_simIntervalMs(250), m_isReturnHome(true), m_homeIdleNow(false), m_simulateMovement(true)
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
    connect(m_simTimer, &QTimer::timeout, this, [this]()
            { stepRobots(); });

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
    if (m_simulateMovement && m_simTimer && !m_simTimer->isActive())
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

void RobotController::setEnableReturnHome(bool enable)
{
    m_isReturnHome = enable;
    if (m_scheduler)
    {
        m_scheduler->setEnableReturnHome(enable);
    }
}

bool RobotController::isReturnHomeEnabled() const
{
    return m_isReturnHome;
}

bool RobotController::returnIdleRobotsToHome()
{
    // 只让空闲机器人回原点，不打断正在执行任务的机器人
    bool any = false;
    for (int id : m_robotManager->getAllRobotIds())
    {
        const Robot *r = m_robotManager->getRobot(id);
        if (r && r->getStatus() == RobotStatus::Idle)
        {
            if (std::abs(r->getPx()) > 1e-3f || std::abs(r->getPy()) > 1e-3f)
                any = true;
        }
    }
    if (any)
    {
        m_homeIdleNow = true;
        // 若未启动调度器，也让移动模拟跑起来，使“全部回原点”能持续移动到位
        if (m_simulateMovement && m_simTimer && !m_simTimer->isActive())
            m_simTimer->start();
    }
    return any;
}

void RobotController::setSimulationMode(bool simulate)
{
    m_simulateMovement = simulate;
    // 关闭模拟时停止移动定时器（等待真实数据源上报位置）
    if (!simulate && m_simTimer)
        m_simTimer->stop();

    if (simulate)
        emit logMessage("[运行模式] 模拟机器人：由系统自动移动机器人以演示调度", 0);
    else
        emit logMessage("[运行模式] TCP 接入真实机器人：位置将由通信层上报（当前为空壳，未实现）", 3);
}

bool RobotController::isSimulationMode() const
{
    return m_simulateMovement;
}

// ========== 移动模拟（让任务能演示完成） ==========

void RobotController::stepRobots()
{
    if (!m_simTimer || !m_simTimer->isActive())
        return;
    if (!m_simulateMovement)
        return; // TCP 模式：位置由真实数据更新，不做模拟移动

    const float dt = m_simIntervalMs / 1000.0f;
    const float defaultSpeed = 8.0f;   // 无速度配置时的模拟速度(单位/秒)
    const float arriveAt = 0.5f;        // 视为到达某段目标的判定距离

    bool allIdleHome = true;            // 用于清除一次性"全部回原点"标记
    const QList<int> ids = m_robotManager->getAllRobotIds();
    for (int id : ids)
    {
        Robot *r = m_robotManager->getRobot(id);
        if (!r)
            continue;

        // 非执行状态的机器人，清掉"起点/终点"相位记录(便于下次重新从起点走)
        if (r->getStatus() != RobotStatus::Busy)
        {
            m_robotTaskPhase.remove(id);
            m_robotStartDone.remove(id);
        }

        if (r->getStatus() == RobotStatus::Busy)
        {
            // 执行期间复位"回程日志"标记，便于完成后再回原点时重新打印
            m_robotReturnStage[id] = 0;

            // —— 执行任务：先到任务起点，再到任务终点(到过起点后锁定终点) ——
            const int taskId = r->getTask();
            if (taskId < 0)
                continue;
            const Task *task = m_taskManager->getTask(taskId);
            if (!task)
                continue;

            // 换任务时重置"是否已到起点"标记，并打印"前往起点"
            if (m_robotTaskPhase.value(id) != taskId)
            {
                m_robotTaskPhase[id] = taskId;
                m_robotStartDone[id] = false;
                emit logMessage("[调度] 机器人 " + QString::number(id) +
                                    " 前往任务 " + QString::number(taskId) +
                                    " 起点(" + QString::number(task->getStartX(), 'f', 1) + "," +
                                    QString::number(task->getStartY(), 'f', 1) + ")",
                                1);
            }

            float tx, ty;
            if (!m_robotStartDone.value(id))
            {
                float dStart = std::sqrt(std::pow(r->getPx() - task->getStartX(), 2) +
                                         std::pow(r->getPy() - task->getStartY(), 2));
                if (dStart <= arriveAt)
                {
                    m_robotStartDone[id] = true; // 到过起点，锁定去终点
                    emit logMessage("[调度] 机器人 " + QString::number(id) +
                                        " 已到任务 " + QString::number(taskId) +
                                        " 起点，正在前往终点(" +
                                        QString::number(task->getEndX(), 'f', 1) + "," +
                                        QString::number(task->getEndY(), 'f', 1) + ")",
                                    1);
                }
                else
                {
                    tx = task->getStartX();
                    ty = task->getStartY();
                }
            }

            if (m_robotStartDone.value(id))
            {
                tx = task->getEndX();
                ty = task->getEndY();
            }

            const float px = r->getPx();
            const float py = r->getPy();
            const float dx = tx - px;
            const float dy = ty - py;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < arriveAt)
            {
                if (!m_robotStartDone.value(id) && std::abs(dx) < 1e-3f && std::abs(dy) < 1e-3f)
                    continue; // 尚在起点等待
                continue;     // 已在终点附近，等调度器判定完成
            }

            const float speed = r->getSpeed() > 0.0f ? r->getSpeed() : defaultSpeed;
            const float step = speed * dt;
            const float move = step > dist ? dist : step;
            m_robotManager->updateRobotPosition(id, px + dx / dist * move, py + dy / dist * move);
            continue;
        }

        if (r->getStatus() != RobotStatus::Idle)
            continue; // 故障/离线/充电等不移动

        // —— 空闲机器人：可能回原点(自动开关 或 手动“全部回原点”) ——
        const float px = r->getPx();
        const float py = r->getPy();
        const bool away = (std::abs(px) > 1e-3f || std::abs(py) > 1e-3f);
        if (away)
            allIdleHome = false; // 还有空闲机器人没到原点，先不清一次性标记

        if (!m_isReturnHome && !m_homeIdleNow)
        {
            m_robotReturnStage[id] = 0; // 不在回程，复位标记
            continue;                   // 既不自动回、也没手动触发 → 停在原地
        }

        // 回程阶段日志(避免每拍重复)
        int &stage = m_robotReturnStage[id];
        if (away && stage == 0)
        {
            stage = 1;
            emit logMessage("[调度] 机器人 " + QString::number(id) +
                                " 任务已结束，正在返回原点",
                            1);
        }

        // 向原点(0,0)移动
        const float dx = 0.0f - px;
        const float dy = 0.0f - py;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < arriveAt)
        {
            if (away)
            {
                m_robotManager->updateRobotPosition(id, 0.0f, 0.0f);
                if (stage == 1)
                {
                    stage = 2;
                    emit logMessage("[调度] 机器人 " + QString::number(id) +
                                        " 已回到原点",
                                    1);
                }
            }
            continue;
        }

        const float speed = r->getSpeed() > 0.0f ? r->getSpeed() : defaultSpeed;
        const float step = speed * dt;
        const float move = step > dist ? dist : step;
        m_robotManager->updateRobotPosition(id, px + dx / dist * move, py + dy / dist * move);
    }

    // 手动“全部回原点”只有在所有空闲机器人都不在原点之外时才清除，
    // 避免只移动一步就被复位
    if (m_homeIdleNow && allIdleHome)
        m_homeIdleNow = false;
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
    return dm.saveAllData(robots, tasks, 8888, interval, m_isReturnHome);
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
    bool enableReturnHome = true;
    if (!dm.loadAllData(robots, tasks, port, interval, enableReturnHome))
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

    setEnableReturnHome(enableReturnHome);

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