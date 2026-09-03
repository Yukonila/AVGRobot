#include "robotManager.h"

RobotManager::RobotManager(QObject *parent)
    : QObject(parent), m_lowBatteryThreshold(20)
{
}

RobotManager::~RobotManager()
{
    clearAll();
}

// ========== 机器人管理 ==========

bool RobotManager::addRobot(const Robot &_bot)
{
    int id = _bot.getId();

    if (id < 0)
    {
        emit logMessage("[RobotManager] 不可用的RobotId: " + QString::number(id), 2);
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);

        if (m_robot.contains(id))
        {
            emit logMessage("[RobotManager] RobotId " + QString::number(id) + " 已经被使用", 2);
            return false;
        }

        m_robot[id] = _bot;
    }

    emit robotAdded(id);
    emit logMessage("[RobotManager] Robot " + QString::number(id) + " 已添加", 1);
    return true;
}

bool RobotManager::addRobot(int id, const QString &ip)
{
    if (id < 0)
    {
        emit logMessage("[RobotManager] 不可用的RobotId: " + QString::number(id), 2);
        return false;
    }
    if (!Robot::isValidIp(ip))
    {
        emit logMessage("[RobotManager] 不可用的RobotIP: " + ip, 2);
        return false;
    }

    Robot robot;
    robot.setAll(id, 0.0f, 0.0f, 100, 0.0f, -1, RobotStatus::Idle, ip);

    return addRobot(robot);
}

bool RobotManager::removeRobot(int id)
{
    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(id);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(id) + " 不存在", 2);
            return false;
        }

        if (it.value().getTask() != -1)
        {
            emit logMessage("[RobotManager] Robot " + QString::number(id) +
                                " 正在执行任务 " + QString::number(it.value().getTask()) + "，无法移除",
                            2);
            return false;
        }

        m_robot.erase(it);
    }

    emit robotRemoved(id);
    emit logMessage("[RobotManager] Robot " + QString::number(id) + " 已移除", 1);
    return true;
}

void RobotManager::clearAll()
{
    QList<int> ids;

    {
        QMutexLocker locker(&m_mutex);

        int busyCount = 0;
        for (auto it = m_robot.begin(); it != m_robot.end(); ++it)
        {
            if (it.value().getTask() != -1)
            {
                busyCount++;
            }
        }

        if (busyCount > 0)
        {
            emit logMessage("[RobotManager] 无法清空: " + QString::number(busyCount) +
                                " 个机器人正在执行任务",
                            2);
            return;
        }

        ids = m_robot.keys();
        m_robot.clear();
    }

    for (int id : ids)
    {
        emit robotRemoved(id);
    }

    emit logMessage("[RobotManager] 所有机器人已清空，共移除 " +
                        QString::number(ids.size()) + " 个机器人",
                    1);
}

Robot *RobotManager::getRobot(int id)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_robot.find(id);
    if (it == m_robot.end())
    {
        return nullptr;
    }
    return &it.value();
}

const Robot *RobotManager::getRobot(int id) const
{
    QMutexLocker locker(&m_mutex);

    auto it = m_robot.find(id);
    if (it == m_robot.end())
    {
        return nullptr;
    }
    return &it.value();
}

bool RobotManager::hasRobot(int id) const
{
    QMutexLocker locker(&m_mutex);
    return m_robot.contains(id);
}

int RobotManager::getRobotCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_robot.size();
}

QList<int> RobotManager::getAllRobotIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_robot.keys();
}

QList<Robot> RobotManager::getAllRobots() const
{
    QMutexLocker locker(&m_mutex);
    return m_robot.values();
}

// ========== 核心更新方法 ==========

bool RobotManager::updateRobotPosition(int id, float x, float y)
{
    bool changed = false;
    bool exists = false;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(id);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(id) + " 不存在", 2);
            return false;
        }

        Robot &robot = it.value();

        if (robot.getPx() != x || robot.getPy() != y)
        {
            robot.setPx(x);
            robot.setPy(y);
            changed = true;
        }
        exists = true;
    }

    if (exists && changed)
    {
        emit robotPositionChanged(id, x, y);
    }

    return exists;
}

bool RobotManager::updateRobotBattery(int id, int battery)
{
    bool changed = false;
    bool exists = false;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(id);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(id) + " 不存在", 2);
            return false;
        }

        Robot &robot = it.value();

        if (robot.getBattery() != battery)
        {
            robot.setBattery(battery);
            changed = true;
        }
        exists = true;
    }

    if (exists)
    {
        if (changed)
        {
            emit robotBatteryChanged(id, battery);
        }

        if (battery < m_lowBatteryThreshold && battery >= 0)
        {
            emit robotLowBattery(id, battery);
            emit logMessage("[RobotManager] Robot " + QString::number(id) +
                                " 低电量告警: " + QString::number(battery) + "%",
                            2);
        }
    }

    return exists;
}

bool RobotManager::updateRobotSpeed(int id, float speed)
{
    bool changed = false;
    bool exists = false;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(id);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(id) + " 不存在", 2);
            return false;
        }

        Robot &robot = it.value();

        if (robot.getSpeed() != speed)
        {
            robot.setSpeed(speed);
            changed = true;
        }
        exists = true;
    }

    if (exists && changed)
    {
        emit robotSpeedChanged(id, speed);
    }

    return exists;
}

bool RobotManager::updateRobotStatus(int id, RobotStatus status)
{
    bool changed = false;
    bool exists = false;
    RobotStatus oldstatus = RobotStatus::None;
    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(id);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(id) + " 不存在", 2);
            return false;
        }
        Robot &robot = it.value();
        oldstatus = robot.getStatus();

        if (oldstatus != status)
        {
            robot.setStatus(status);
            changed = true;
        }
        exists = true;
    }
    if (exists && changed)
    {
        emit robotStatusChanged(id, oldstatus, status);
        if (status == RobotStatus::Error)
        {
            emit robotFaultDetected(id, 0);
            emit logMessage("[RobotManager] Robot " + QString::number(id) + "故障状态！", 2);
        }
        emit logMessage("[RobotManager] Robot " + QString::number(id) + "状态变化：" +
                            QString::number((int)oldstatus) + "->" + QString::number((int)status),
                        1);
    }
    return exists;
}

bool RobotManager::updateRobotTask(int id, int taskId)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_robot.find(id);
    if (it == m_robot.end())
    {
        emit logMessage("[RobotManager] Robot " + QString::number(id) + " 不存在", 2);
        return false;
    }
    it.value().setTask(taskId);
    return true;
}

bool RobotManager::updateRobotIp(int id, const QString &ip)
{
    bool changed = false;
    bool exists = false;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(id);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(id) + " 不存在", 2);
            return false;
        }

        Robot &robot = it.value();

        if (robot.getId() != ip)
        {
            robot.setIp(ip);
            changed = true;
        }
        exists = true;
    }

    if (exists && changed)
    {
        emit robotIpChanged(id, ip);
    }

    return exists;
}

bool RobotManager::updateRobotStatus(int id, float x, float y, int battery, float speed, RobotStatus status)
{
    bool exists = false;
    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(id);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(id) + " 不存在", 2);
            return false;
        }
        exists = true;
    }
    if (!exists)
    {
        return false;
    }
    updateRobotPosition(id, x, y);
    updateRobotBattery(id, battery);
    updateRobotSpeed(id, speed);
    if (status != RobotStatus::None)
    {
        updateRobotStatus(id, status);
    }
    return true;
}

// 按照状态返回id列表
QList<int> RobotManager::getRobotsByStatus(RobotStatus status) const
{
    QMutexLocker locker(&m_mutex);

    QList<int> result;
    for (auto it = m_robot.begin(); it != m_robot.end(); it++)
    {
        if (it.value().getStatus() == status)
        {
            result.append(it.key());
        }
    }
    return result;
}

QList<int> RobotManager::getOnlineRobots() const
{
    QMutexLocker locker(&m_mutex);

    QList<int> result;
    for (auto it = m_robot.begin(); it != m_robot.end(); it++)
    {
        RobotStatus statues = it.value().getStatus();
        if (statues != RobotStatus::Offline && statues != RobotStatus::None)
        {
            result.append(it.key());
        }
    }
    return result;
}

QList<int> RobotManager::getLowBatteryRobots(int threshold) const
{
    QMutexLocker locker(&m_mutex);

    QList<int> result;
    for (auto it = m_robot.begin(); it != m_robot.end(); it++)
    {
        if (it.value().getBattery() < threshold && it.value().getBattery() >= 0)
        {
            result.append(it.key());
        }
    }
    return result;
}

// 按照负载排序
QList<int> RobotManager::getRobotsSortedByLoad() const
{
    QMutexLocker locker(&m_mutex);

    QList<int> ids = m_robot.keys();

    std::sort(ids.begin(), ids.end(), [this](int a, int b)
              { bool busyA = (m_robot[a].getTask() != -1);
                bool busyB = (m_robot[b].getTask() != -1);
                return busyA > busyB ; });

    return ids;
}

QList<int> RobotManager::getIdleRobots() const
{
    return getRobotsByStatus(RobotStatus::Idle);
}

QList<int> RobotManager::getFaultRobots() const
{
    return getRobotsByStatus(RobotStatus::Error);
}

QList<int> RobotManager::getBusyRobots() const
{
    return getRobotsByStatus(RobotStatus::Busy);
}

// 任务相关
bool RobotManager::assignTaskToRobot(int robotId, int taskId)
{
    RobotStatus oldStatus = RobotStatus::None;
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(robotId);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(robotId) + " 不存在", 2);
            return false;
        }

        Robot &robot = it.value();

        if (robot.getTask() != -1)
        {
            emit logMessage("[RobotManager] Robot " + QString::number(robotId) +
                                " 正在执行任务 " + QString::number(robot.getTask()) + "，无法分配新任务",
                            2);
            return false;
        }

        if (robot.getStatus() != RobotStatus::Idle)
        {
            emit logMessage("[RobotManager] Robot " + QString::number(robotId) +
                                " 不是空闲状态 (status:" + QString::number((int)robot.getStatus()) + ")",
                            2);
            return false;
        }
        oldStatus = robot.getStatus();
        robot.setTask(taskId);
        robot.setStatus(RobotStatus::Busy);
        success = true;
    }

    if (success)
    {
        emit robotTaskAssigned(robotId, taskId);
        emit robotStatusChanged(robotId, oldStatus, RobotStatus::Busy);
        emit logMessage("[RobotManager] 任务 " + QString::number(taskId) +
                            " 已分配给 Robot " + QString::number(robotId),
                        1);
    }

    return success;
}

bool RobotManager::finishRobotTask(int robotId)
{
    int taskId = -1;
    RobotStatus oldStatus = RobotStatus::None;
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(robotId);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(robotId) + " 不存在", 2);
            return false;
        }

        Robot &robot = it.value();
        taskId = robot.getTask();

        if (taskId == -1)
        {
            emit logMessage("[RobotManager] Robot " + QString::number(robotId) + " 没有任务可完成", 2);
            return false;
        }

        oldStatus = robot.getStatus();
        robot.setTask(-1);
        robot.setStatus(RobotStatus::Idle);
        success = true;
    }

    if (success)
    {
        emit robotTaskFinished(robotId, taskId);
        emit robotStatusChanged(robotId, oldStatus, RobotStatus::Idle);
        emit logMessage("[RobotManager] Robot " + QString::number(robotId) +
                            " 完成任务 " + QString::number(taskId),
                        1);
    }

    return success;
}

bool RobotManager::cancelRobotTask(int robotId)
{
    int taskId = -1;
    RobotStatus oldStatus = RobotStatus::None;
    bool success = false;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_robot.find(robotId);
        if (it == m_robot.end())
        {
            emit logMessage("[RobotManager] Robot " + QString::number(robotId) + " 不存在", 2);
            return false;
        }

        Robot &robot = it.value();
        taskId = robot.getTask();

        if (taskId == -1)
        {
            emit logMessage("[RobotManager] Robot " + QString::number(robotId) + " 没有任务可取消", 2);
            return false;
        }

        oldStatus = robot.getStatus();
        robot.setTask(-1);
        robot.setStatus(RobotStatus::Idle);
        success = true;
    }

    if (success)
    {
        emit robotTaskCancelled(robotId, taskId);
        emit robotStatusChanged(robotId, oldStatus, RobotStatus::Idle);
        emit logMessage("[RobotManager] Robot " + QString::number(robotId) +
                            " 任务 " + QString::number(taskId) + " 已取消",
                        1);
    }

    return success;
}

int RobotManager::getRobotCurrentTask(int robotId) const
{
    QMutexLocker locker(&m_mutex);

    auto it = m_robot.find(robotId);
    if (it == m_robot.end())
    {
        return -1;
    }
    return it.value().getTask();
}

bool RobotManager::isRobotBusy(int robotId) const
{
    QMutexLocker locker(&m_mutex);

    auto it = m_robot.find(robotId);
    if (it == m_robot.end())
    {
        return false;
    }
    return it.value().getTask() != -1;
}

// 统计相关

int RobotManager::getCountByStatus(RobotStatus status) const
{
    return getRobotsByStatus(status).count();
}

int RobotManager::getIdleCount() const
{
    return getCountByStatus(RobotStatus::Idle);
}

int RobotManager::getBusyCount() const
{
    return getCountByStatus(RobotStatus::Busy);
}

int RobotManager::getFaultCount() const
{
    return getCountByStatus(RobotStatus::Error);
}

int RobotManager::getLowBatteryCount(int threshold) const
{
    return getLowBatteryRobots(threshold).count();
}

int RobotManager::getOnlineCount() const
{
    QMutexLocker locker(&m_mutex);

    int count = 0;
    for (auto it = m_robot.begin(); it != m_robot.end(); it++)
    {
        RobotStatus status = it.value().getStatus();
        if (status != RobotStatus::Offline && status != RobotStatus::None)
        {
            count++;
        }
    }
    return count;
}

// ========== 调试 ==========

QString RobotManager::printAllRobots() const
{
    QMutexLocker locker(&m_mutex);

    if (m_robot.isEmpty())
    {
        return "=== 没有机器人 ===";
    }

    int idle = 0, busy = 0, fault = 0, online = 0, lowBattery = 0;

    QString body;
    for (auto it = m_robot.begin(); it != m_robot.end(); ++it)
    {
        const Robot &r = it.value();
        RobotStatus st = r.getStatus();

        if (st == RobotStatus::Idle)
            idle++;
        else if (st == RobotStatus::Busy)
            busy++;
        else if (st == RobotStatus::Error)
            fault++;
        if (st != RobotStatus::Offline && st != RobotStatus::None)
            online++;
        if (r.getBattery() < m_lowBatteryThreshold && r.getBattery() >= 0)
            lowBattery++;

        body += r.printRobot() + "\n";
    }

    QString result;
    result += "=== 机器人列表 (总数: " + QString::number(m_robot.size()) + ") ===\n";
    result += "空闲: " + QString::number(idle) + " | ";
    result += "忙碌: " + QString::number(busy) + " | ";
    result += "故障: " + QString::number(fault) + " | ";
    result += "在线: " + QString::number(online) + "\n";
    result += "低电量(<" + QString::number(m_lowBatteryThreshold) + "%): " +
              QString::number(lowBattery) + "\n\n";
    result += body;
    result += "\n";
    return result;
}

QString RobotManager::printRobotBrief(int id) const
{
    QMutexLocker locker(&m_mutex);

    auto it = m_robot.find(id);
    if (it == m_robot.end())
    {
        return "Robot " + QString::number(id) + " not found";
    }

    const Robot &robot = it.value();
    return QString("Robot #%1 | 位置:(%2,%3) | 电量:%4% | 状态:%5 | 任务:%6")
        .arg(id)
        .arg(robot.getPx(), 0, 'f', 1)
        .arg(robot.getPy(), 0, 'f', 1)
        .arg(robot.getBattery())
        .arg((int)robot.getStatus())
        .arg(robot.getTask());
}