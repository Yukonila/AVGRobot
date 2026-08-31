#include "robotManager.h"

RobotManager::RobotManager(QObject *parent)
    : QObject(parent)
{
}

RobotManager::~RobotManager()
{
    clearRobot();
}

bool RobotManager::addRobot(const Robot &_bot)
{
    QMutexLocker locker(&m_mutex);
    int id = _bot.getId();
    if (id < 0)
    {
        emit logMessage("[RobotManager] 不可用的RobotId", 2);
        return false;
    }
    if (m_robot.contains(id))
    {
        emit logMessage("[RobotManager] RobotId已经被使用", 2);
        return false;
    }
    m_robot[id] = _bot;

    emit robotAdded(id);
    emit logMessage("[RobotManager] " + QString::number(id) + "已经添加", 1);
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
    QMutexLocker locker(&m_mutex);
    if (!m_robot.contains(id))
    {
        emit logMessage("[RobotManager] 没有查找到RobotId: " + QString::number(id), 2);
        return false;
    }
    const Robot &robot = m_robot[id];
    if (robot.getTask() != -1)
    {
        emit logMessage("[RobotManager] Robot " + QString::number(id) +
                            " 正在执行任务 " + QString::number(robot.getTask()) + "，无法移除",
                        2);
        return false;
    }
    m_robot.remove(id);
    emit robotRemoved(id);
    emit logMessage("[RobotManager] Robot " + QString::number(id) + " 已移除", 1);
    return true;
}

void RobotManager::clearRobot()
{
    QMutexLocker locker(&m_mutex);
    m_robot.clear();
}
