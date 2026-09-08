#include "robotcontroller.h"
#include "Data/datamanager.h"
#include <QDebug>
#include <QPoint>
#include <QRandomGenerator>
#include <cmath>

RobotController::RobotController(QObject *parent)
    : QObject(parent), m_robotManager(new RobotManager(this)), m_taskManager(new TaskManager(this)), m_scheduler(nullptr), m_simTimer(nullptr), m_simIntervalMs(250), m_isReturnHome(true), m_homeIdleNow(false), m_simulateMovement(true), m_maxRobotSpeed(12.0f), m_lowChargeLevel(30.0f), m_chargePerSec(8.0f), m_drainPerUnit(0.22f)
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

    // 默认充电桩 = 原点
    addDefaultCharger();

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
    if (!m_robotManager->addRobot(id, ip))
        return false;
    // 新机器人默认放到"充电桩为圆心、圆内随机"的位置(避障)
    QPointF sp = nextSpawnPos();
    Robot *r = m_robotManager->getRobot(id);
    if (r)
    {
        r->setPx((float)sp.x());
        r->setPy((float)sp.y());
        emit robotPositionChanged(id, (float)sp.x(), (float)sp.y());
    }
    return true;
}

QPointF RobotController::nextSpawnPos() const
{
    // 圆心取第一个充电桩(默认画布中部的家)，没有则用中点
    QPointF center = m_chargers.isEmpty() ? homePoint() : m_chargers.first();
    const float radius = 4.0f;
    for (int attempt = 0; attempt < 24; ++attempt)
    {
        float ang = QRandomGenerator::global()->generateDouble() * 6.2831853f;
        float rad = 0.6f + QRandomGenerator::global()->generateDouble() * radius;
        float cx = (float)center.x() + rad * std::cos(ang);
        float cy = (float)center.y() + rad * std::sin(ang);
        if (cx >= 0.0f && cy >= 0.0f && cx < m_gridCols && cy < m_gridRows && !isBlockedWorld(cx, cy))
            return QPointF(cx, cy);
    }
    return center;
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
    if (speed < 0.0f)
        speed = 0.0f;
    if (speed > m_maxRobotSpeed)
        speed = m_maxRobotSpeed; // 速度上限
    return m_robotManager->updateRobotSpeed(id, speed);
}

bool RobotController::updateRobotAccel(int id, float accel)
{
    Robot *r = m_robotManager->getRobot(id);
    if (!r)
        return false;
    r->setAccel(accel);
    return true;
}

bool RobotController::updateRobotMaxLoad(int id, int maxLoad)
{
    Robot *r = m_robotManager->getRobot(id);
    if (!r)
        return false;
    r->setMaxLoad(maxLoad);
    return true;
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

bool RobotController::removeNewestTask()
{
    QList<int> ids = m_taskManager->getAllTaskIds();
    if (ids.isEmpty())
        return false;
    int mx = ids.first();
    for (int id : ids)
        if (id > mx)
            mx = id;
    return m_taskManager->removeTask(mx);
}

void RobotController::clearAllTasks()
{
    m_taskManager->clearAll();
}

void RobotController::removeAllRobots()
{
    QList<int> ids = m_robotManager->getAllRobotIds();
    for (int id : ids)
    {
        const Robot *r = m_robotManager->getRobot(id);
        if (r && r->getTask() >= 0)
            cancelExecutingTask(r->getTask()); // 先释放忙碌机器人
        m_robotManager->removeRobot(id);
    }
}

Task *RobotController::getTask(int taskId)
{
    return m_taskManager->getTask(taskId);
}

bool RobotController::cancelExecutingTask(int taskId)
{
    Task *t = m_taskManager->getTask(taskId);
    if (!t)
        return false;
    int rid = t->getAssignedRobotId();
    if (rid >= 0 && m_robotManager->getRobot(rid))
        m_robotManager->finishRobotTask(rid); // 释放机器人
    return m_taskManager->cancelTask(taskId);
}

bool RobotController::recycleTask(int taskId)
{
    Task *t = m_taskManager->getTask(taskId);
    if (!t)
        return false;
    if (t->isFinished())
        return false;
    int rid = t->getAssignedRobotId();
    if (rid >= 0 && m_robotManager->getRobot(rid))
        m_robotManager->finishRobotTask(rid); // 释放机器人
    return m_taskManager->reassignTask(taskId); // 放回待分配队列
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

// ========== 地图网格 / 可达性 ==========

void RobotController::setMapGrid(int cols, int rows, const QVector<char> &obstacles)
{
    m_gridCols = qMax(1, cols);
    m_gridRows = qMax(1, rows);
    m_obstacles = obstacles;
    if (m_obstacles.size() < m_gridCols * m_gridRows)
        m_obstacles.fill(0, m_gridCols * m_gridRows);
}

bool RobotController::isBlockedWorld(float x, float y) const
{
    int cx = (int)std::floor(x);
    int cy = (int)std::floor(y);
    if (cx < 0 || cy < 0 || cx >= m_gridCols || cy >= m_gridRows)
        return true; // 界外视为不可行
    return m_obstacles.value(cellIndex(cx, cy), 1) == 1;
}

bool RobotController::isReachable(float ax, float ay, float bx, float by) const
{
    // 世界坐标→格子
    auto toCell = [](float w) -> int { return (int)std::floor(w); };
    int sx = toCell(ax), sy = toCell(ay);
    int gx = toCell(bx), gy = toCell(by);

    auto inBounds = [&](int x, int y)
    { return x >= 0 && y >= 0 && x < m_gridCols && y < m_gridRows; };

    if (!inBounds(sx, sy) || !inBounds(gx, gy))
        return false;
    if (m_obstacles.value(cellIndex(sx, sy), 1) == 1 || m_obstacles.value(cellIndex(gx, gy), 1) == 1)
        return false;
    if (sx == gx && sy == gy)
        return true;

    // BFS 4 邻域连通性
    QVector<int> dist(m_gridCols * m_gridRows, -1);
    QVector<QPoint> que;
    dist[cellIndex(sx, sy)] = 0;
    que.append(QPoint(sx, sy));
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    int head = 0;
    while (head < que.size())
    {
        QPoint cur = que[head++];
        if (cur.x() == gx && cur.y() == gy)
            return true;
        for (int k = 0; k < 4; ++k)
        {
            int nx = cur.x() + dx[k];
            int ny = cur.y() + dy[k];
            if (!inBounds(nx, ny))
                continue;
            if (m_obstacles.value(cellIndex(nx, ny), 1) == 1)
                continue;
            if (dist[cellIndex(nx, ny)] >= 0)
                continue;
            dist[cellIndex(nx, ny)] = dist[cellIndex(cur.x(), cur.y())] + 1;
            que.append(QPoint(nx, ny));
        }
    }
    return false;
}

QList<QPointF> RobotController::planPathWorld(float ax, float ay, float bx, float by) const
{
    QList<QPointF> out;
    auto toCell = [](float w) -> int { return (int)std::floor(w); };
    int sx = toCell(ax), sy = toCell(ay);
    int gx = toCell(bx), gy = toCell(by);
    auto inBounds = [&](int x, int y)
    { return x >= 0 && y >= 0 && x < m_gridCols && y < m_gridRows; };

    if (!inBounds(sx, sy) || !inBounds(gx, gy))
        return out;
    if (m_obstacles.value(cellIndex(sx, sy), 1) == 1 || m_obstacles.value(cellIndex(gx, gy), 1) == 1)
        return out;
    if (sx == gx && sy == gy)
    {
        out.append(QPointF(sx + 0.5f, sy + 0.5f));
        return out;
    }

    // BFS + 前驱回溯（4 邻域）
    QVector<int> pxv(m_gridCols * m_gridRows, -1), pyv(m_gridCols * m_gridRows, -1);
    QVector<char> vis(m_gridCols * m_gridRows, 0);
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    QList<QPoint> que;
    int head = 0;
    vis[cellIndex(sx, sy)] = 1;
    que.append(QPoint(sx, sy));
    bool found = false;
    while (head < que.size() && !found)
    {
        QPoint cur = que[head++];
        for (int k = 0; k < 4 && !found; ++k)
        {
            int nx = cur.x() + dx[k];
            int ny = cur.y() + dy[k];
            if (!inBounds(nx, ny))
                continue;
            if (m_obstacles.value(cellIndex(nx, ny), 1) == 1)
                continue;
            int nidx = cellIndex(nx, ny);
            if (vis[nidx])
                continue;
            vis[nidx] = 1;
            pxv[nidx] = cur.x();
            pyv[nidx] = cur.y();
            if (nx == gx && ny == gy)
            {
                found = true;
                break;
            }
            que.append(QPoint(nx, ny));
        }
    }
    if (!found)
        return out;

    // 回溯路径(不含起点，含终点)，再反向
    QList<QPoint> rev;
    int cx = gx, cy = gy;
    while (cx != sx || cy != sy)
    {
        rev.prepend(QPoint(cx, cy));
        int pxx = pxv[cellIndex(cx, cy)];
        int pyy = pyv[cellIndex(cx, cy)];
        cx = pxx;
        cy = pyy;
    }
    out.append(QPointF(sx + 0.5f, sy + 0.5f)); // 起点中心
    for (const QPoint &p : rev)
        out.append(QPointF(p.x() + 0.5f, p.y() + 0.5f));
    return out;
}

// ========== 电量 / 充电桩(模拟) ==========

float RobotController::maxSpeed() const { return m_maxRobotSpeed; }
float RobotController::lowChargeLevel() const { return m_lowChargeLevel; }

QList<QPointF> RobotController::chargers() const { return m_chargers; }

void RobotController::addCharger(float x, float y)
{
    m_chargers.append(QPointF(x, y));
    emit logMessage("[充电桩] 新增充电桩 @(" + QString::number(x, 'f', 1) + "," +
                        QString::number(y, 'f', 1) + ")",
                    0);
}

void RobotController::clearChargers()
{
    m_chargers.clear();
    m_chargeTarget.clear();
}

void RobotController::addDefaultCharger()
{
    if (m_chargers.isEmpty())
        m_chargers.append(homePoint()); // 家的位置(画布中部)
}

QPointF RobotController::nearestCharger(float x, float y) const
{
    if (m_chargers.isEmpty())
        return homePoint();
    QPointF best = m_chargers.first();
    float bestD = 1e30f;
    for (const QPointF &c : m_chargers)
    {
        float d = std::sqrt((c.x() - x) * (c.x() - x) + (c.y() - y) * (c.y() - y));
        if (d < bestD)
        {
            bestD = d;
            best = c;
        }
    }
    return best;
}

// 移动后按距离与速度掉电
void RobotController::drainBattery(int robotId, float distance, float speed)
{
    Robot *r = m_robotManager->getRobot(robotId);
    if (!r || distance <= 0.0f)
        return;
    int batt = r->getBattery();
    if (batt <= 0)
        return;
    float ratio = m_maxRobotSpeed > 0.0f ? qMin(speed / m_maxRobotSpeed, 1.0f) : 0.5f;
    // 更快 → 单位距离掉电更多
    float drop = distance * m_drainPerUnit * (0.5f + 0.5f * ratio);
    int nb = qMax(0, qRound((float)batt - drop));
    if (nb != batt)
    {
        r->setBattery(nb);
        emit robotBatteryChanged(robotId, nb);
    }
}

// 空闲且电量低于阈值 → 转入充电状态，返回 true
bool RobotController::goChargeIfLow(int robotId)
{
    Robot *r = m_robotManager->getRobot(robotId);
    if (!r || r->getStatus() != RobotStatus::Idle)
        return false;
    if (r->getBattery() > (int)m_lowChargeLevel)
        return false;

    QPointF target = nearestCharger(r->getPx(), r->getPy());
    m_chargeTarget[robotId] = target;
    m_robotManager->updateRobotStatus(robotId, RobotStatus::Charging); // 置充电，调度不再分配
    emit logMessage("[充电] 机器人 " + QString::number(robotId) +
                        " 电量 " + QString::number(r->getBattery()) +
                        "% 过低，前往充电桩(" + QString::number(target.x(), 'f', 1) + "," +
                        QString::number(target.y(), 'f', 1) + ") 充电",
                    3);
    return true;
}

bool RobotController::loadRobotsOnly()
{
    DataManager dm(this);
    connect(&dm, &DataManager::logMessage, this, &RobotController::logMessage);

    QList<Robot> robots;
    QList<Task> tasks;
    int port = 8888, interval = 3000;
    bool retHome = true;
    if (!dm.loadAllData(robots, tasks, port, interval, retHome))
    {
        emit logMessage("[RobotController] 载入机器人失败(数据文件不存在)", 2);
        return false;
    }

    // 只替换机器人，任务不载入(仍用当前运行中的任务)
    m_robotManager->clearAll();
    for (const Robot &r : robots)
        m_robotManager->addRobot(r);

    emit logMessage("[RobotController] 已载入 " + QString::number(robots.size()) + " 台机器人(不含任务)",
                    0);
    return true;
}

// ========== 移动模拟（让任务能演示完成） ==========

// 执行中的机器人：沿避障网格路径走(先到任务起点再绕行到终点)，被其它机器人挡住则等待
void RobotController::stepBusyRobot(int id, Robot *r, float dt)
{
    const float defaultSpeed = 8.0f;
    const float arriveTh = 0.35f;

    m_robotReturnStage[id] = 0;
    m_robotTaskPhase.remove(id);
    m_robotStartDone.remove(id);

    const int taskId = r->getTask();
    if (taskId < 0)
        return;
    const Task *task = m_taskManager->getTask(taskId);
    if (!task)
        return;

    // 若任务变化(或未规划)，重算路径：当前格子→任务起点→任务终点
    if (m_robotPlanTask.value(id) != taskId || !m_robotPlan.contains(id))
    {
        QPointF cur(r->getPx(), r->getPy());
        QPointF st(task->getStartX(), task->getStartY());
        QPointF en(task->getEndX(), task->getEndY());
        QList<QPointF> p1 = planPathWorld((float)cur.x(), (float)cur.y(),
                                          (float)st.x(), (float)st.y());
        QList<QPointF> p2 = planPathWorld((float)st.x(), (float)st.y(),
                                          (float)en.x(), (float)en.y());
        QList<QPointF> plan;
        for (const QPointF &pt : p1)
        {
            if (plan.isEmpty() || plan.last() != pt)
                plan.append(pt);
        }
        // 拼接(跳过重复的起点格)
        for (int i = 0; i < p2.size(); ++i)
        {
            if (i == 0 && !plan.isEmpty() && plan.last() == p2[i])
                continue;
            plan.append(p2[i]);
        }
        m_robotPlan[id] = plan;
        m_robotPlanTask[id] = taskId;
        m_robotPlanIdx[id] = 0;
        if (!plan.isEmpty())
            emit logMessage("[调度] 机器人 " + QString::number(id) +
                                " 沿避障路径前往任务 " + QString::number(taskId),
                            1);
    }

    QList<QPointF> &plan = m_robotPlan[id];
    if (plan.isEmpty())
        return; // 不可达(应在创建任务时被拦截)；原地等待

    int &idx = m_robotPlanIdx[id];
    const float speed = r->getSpeed() > 0.0f ? qMin(r->getSpeed(), m_maxRobotSpeed) : defaultSpeed;
    const float step = speed * dt;

    // 跳过已到达的路径点
    while (idx < plan.size())
    {
        QPointF w = plan[idx];
        float dx = w.x() - r->getPx();
        float dy = w.y() - r->getPy();
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < arriveTh)
        {
            ++idx;
            continue;
        }

        float move = step > dist ? dist : step;
        float nx = r->getPx() + dx / dist * move;
        float ny = r->getPy() + dy / dist * move;
        if (robotProximityBlocked(id, nx, ny) || blockedByCharger(id, nx, ny))
            return; // 前方有机器人/被充电桩格拦住 → 等待
        m_robotManager->updateRobotPosition(id, nx, ny);
        drainBattery(id, move, speed);
        return;
    }
}

// 沿避障网格路径向目标(世界坐标)走一步；途中返回 true，已到目标返回 false
bool RobotController::stepToward(int id, float tx, float ty, float speed, float dt)
{
    Robot *r = m_robotManager->getRobot(id);
    if (!r)
        return false;
    const float arriveTh = 0.35f;
    float dist0 = std::sqrt((tx - r->getPx()) * (tx - r->getPx()) + (ty - r->getPy()) * (ty - r->getPy()));
    if (dist0 < arriveTh)
        return false;

    QList<QPointF> path = planPathWorld(r->getPx(), r->getPy(), tx, ty);
    if (path.isEmpty())
        return true; // 暂时无法到达，原地等待

    // 下一个路径点(从自身格到下一格中心)；同格则直接朝目标
    QPointF target = path.last();
    for (int i = 1; i < path.size(); ++i)
    {
        float d = std::sqrt(std::pow(path[i].x() - r->getPx(), 2) +
                            std::pow(path[i].y() - r->getPy(), 2));
        if (d > arriveTh)
        {
            target = path[i];
            break;
        }
    }

    float dx = target.x() - r->getPx();
    float dy = target.y() - r->getPy();
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1e-3f)
        return true;
    const float step = speed * dt;
    const float move = step > dist ? dist : step;
    float nx = r->getPx() + dx / dist * move;
    float ny = r->getPy() + dy / dist * move;
    if (robotProximityBlocked(id, nx, ny) || blockedByCharger(id, nx, ny))
        return true; // 被其它机器人占道/充电桩格拦住 → 等待
    m_robotManager->updateRobotPosition(id, nx, ny);
    return true;
}

// 距其它机器人太近(约一格)则视为被占道，返回 true
bool RobotController::robotProximityBlocked(int id, float nx, float ny) const
{
    const float minDist = 0.8f;
    for (int oid : m_robotManager->getAllRobotIds())
    {
        if (oid == id)
            continue;
        const Robot *o = m_robotManager->getRobot(oid);
        if (!o)
            continue;
        float dx = o->getPx() - nx;
        float dy = o->getPy() - ny;
        if (std::sqrt(dx * dx + dy * dy) < minDist)
            return true;
    }
    return false;
}

// 世界坐标落在某充电桩格内时：只有"正要去该桩充电"的机器人可进入，其余机器人都被拦下
bool RobotController::blockedByCharger(int robotId, float wx, float wy) const
{
    for (const QPointF &ch : m_chargers)
    {
        float d = std::sqrt((ch.x() - wx) * (ch.x() - wx) + (ch.y() - wy) * (ch.y() - wy));
        if (d < 0.5f)
        {
            QPointF tgt = m_chargeTarget.value(robotId, QPointF(-1, -1));
            float dt = std::sqrt((ch.x() - tgt.x()) * (ch.x() - tgt.x()) +
                                 (ch.y() - tgt.y()) * (ch.y() - tgt.y()));
            if (dt < 0.5f)
                return false; // 本机正要去该桩
            return true;      // 其它机器人不可进入充电桩格
        }
    }
    return false;
}

// 破除对头僵局：把低优先级/靠后的机器人横向挪开约半格(避开障碍)
void RobotController::resolveConflicts()
{
    const float tooClose = 0.9f;
    QList<int> ids = m_robotManager->getAllRobotIds();
    QList<int> nudged;
    for (int i = 0; i < ids.size(); ++i)
    {
        for (int j = i + 1; j < ids.size(); ++j)
        {
            const Robot *a = m_robotManager->getRobot(ids[i]);
            const Robot *b = m_robotManager->getRobot(ids[j]);
            if (!a || !b)
                continue;
            float dx = b->getPx() - a->getPx();
            float dy = b->getPy() - a->getPy();
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < 1e-4f)
                continue; // 完全重合：跳过，避免除以零
            if (d > tooClose)
                continue;
            // 让道：执行中者优先不动；否则让 id 较小者不动，另一个挪开
            int mover = ids[j];
            bool aBusy = (a->getStatus() == RobotStatus::Busy);
            bool bBusy = (b->getStatus() == RobotStatus::Busy);
            if (aBusy != bBusy)
                mover = bBusy ? ids[i] : ids[j];
            if (nudged.contains(mover))
                continue;
            const Robot *m = m_robotManager->getRobot(mover);
            if (!m)
                continue;
            // 沿垂直方向挪 0.5 格
            float nx = m->getPx();
            float ny = m->getPy();
            for (int k = 0; k < 4; ++k)
            {
                // 用 a→b 向量做垂直偏移
                float perpX = -dy / d;
                float perpY = dx / d;
                if (k >= 2)
                {
                    perpX = -perpX;
                    perpY = -perpY;
                }
                float tx = m->getPx() + perpX * (1.0f);
                float ty = m->getPy() + perpY * (1.0f);
                if (tx >= 0 && ty >= 0 && tx < m_gridCols && ty < m_gridRows &&
                    !isBlockedWorld(tx, ty))
                {
                    nx = tx;
                    ny = ty;
                    break;
                }
            }
            if (std::abs(nx - m->getPx()) > 1e-3f || std::abs(ny - m->getPy()) > 1e-3f)
            {
                m_robotManager->updateRobotPosition(mover, nx, ny);
                nudged.append(mover);
            }
        }
    }
    if (!nudged.isEmpty())
        emit logMessage("[调度] 已解除 " + QString::number(nudged.size()) + " 处卡位", 1);
}

void RobotController::stepRobots()
{
    if (!m_simTimer || !m_simTimer->isActive())
        return;
    if (!m_simulateMovement)
        return; // TCP 模式：位置由真实数据更新，不做模拟移动

    const float dt = m_simIntervalMs / 1000.0f;
    const float defaultSpeed = 8.0f;   // 无速度配置时的模拟速度(单位/秒)
    const float arriveAt = 0.5f;        // 视为到达某段目标的判定距离

    const QList<int> ids = m_robotManager->getAllRobotIds();
    for (int id : ids)
    {
        Robot *r = m_robotManager->getRobot(id);
        if (!r)
            continue;

        // 非执行状态的机器人，清掉"起点/终点"相位记录与避障路径(便于下次重新规划)
        if (r->getStatus() != RobotStatus::Busy)
        {
            m_robotTaskPhase.remove(id);
            m_robotStartDone.remove(id);
            m_robotPlan.remove(id);
            m_robotPlanTask.remove(id);
            m_robotPlanIdx.remove(id);
        }

        if (r->getStatus() == RobotStatus::Busy)
        {
            stepBusyRobot(id, r, dt); // 沿避障路径走向任务终点
            continue;
        }

        if (r->getStatus() == RobotStatus::Charging)
        {
            // —— 充电中：先沿避障路径到充电桩，到桩后充电 ——
            const float px = r->getPx();
            const float py = r->getPy();
            if (!m_chargeTarget.contains(id))
                m_chargeTarget[id] = nearestCharger(px, py);
            QPointF tg = m_chargeTarget.value(id);
            const float dist = std::sqrt(std::pow(tg.x() - px, 2) + std::pow(tg.y() - py, 2));
            if (dist > arriveAt)
            {
                const float speed = r->getSpeed() > 0.0f ? r->getSpeed() : defaultSpeed;
                stepToward(id, (float)tg.x(), (float)tg.y(), speed, dt);
            }
            else
            {
                // 在充电桩，开始充电
                int batt = r->getBattery();
                int nb = qMin(100, batt + qRound(m_chargePerSec * dt));
                if (nb != batt)
                {
                    r->setBattery(nb);
                    emit robotBatteryChanged(id, nb);
                }
                // 充满 或 (≥80% 且有等待分配的任务) → 恢复可工作
                bool full = batt >= 100;
                bool enough = (m_taskManager->getPendingCount() > 0 && batt >= 80);
                if (full || enough)
                {
                    m_robotManager->updateRobotStatus(id, RobotStatus::Idle);
                    m_chargeTarget.remove(id);
                    emit logMessage("[充电] 机器人 " + QString::number(id) +
                                        (full ? " 已充满，恢复空闲" : " 已充至 80% 以上，有任务待分配，恢复空闲"),
                                    1);
                }
            }
            continue;
        }

        if (r->getStatus() != RobotStatus::Idle)
            continue; // 故障/离线等不移动

        // —— 空闲机器人：低电则去充电；否则回最近充电桩停靠 ——
        if (goChargeIfLow(id))
            continue;

        QPointF ch = nearestCharger(r->getPx(), r->getPy());
        float dch = std::sqrt(std::pow(ch.x() - r->getPx(), 2) + std::pow(ch.y() - r->getPy(), 2));
        if (dch > arriveAt)
        {
            const float speed = r->getSpeed() > 0.0f ? qMin(r->getSpeed(), m_maxRobotSpeed) : defaultSpeed;
            stepToward(id, (float)ch.x(), (float)ch.y(), speed, dt); // 沿避障路径回桩
        }
        // 已到桩：保持空闲停靠(不移动)
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