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

    Robot robot;
    robot.setId(id);
    robot.setIp(ip);
    robot.setStatus(RobotStatus::Idle);
    robot.setBattery(100);
    robot.setPx(0.0f);
    robot.setPy(0.0f);
    robot.setSpeed(0.0f);
    robot.setTask(-1);

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