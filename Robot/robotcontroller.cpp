#include "robotcontroller.h"

RobotController::RobotController(QObject *parent)
    : QObject(parent), m_manager(new RobotManager(this))
{
    // 连接 Manager 的信号到 Controller 的信号（透传）
    connect(m_manager, &RobotManager::logMessage,
            this, &RobotController::logMessage);
    connect(m_manager, &RobotManager::robotAdded,
            this, &RobotController::robotAdded);
    connect(m_manager, &RobotManager::robotRemoved,
            this, &RobotController::robotRemoved);
    connect(m_manager, &RobotManager::robotStatusChanged,
            this, &RobotController::robotStatusChanged);
    connect(m_manager, &RobotManager::robotPositionChanged,
            this, &RobotController::robotPositionChanged);
    connect(m_manager, &RobotManager::robotBatteryChanged,
            this, &RobotController::robotBatteryChanged);
    connect(m_manager, &RobotManager::robotTaskAssigned,
            this, &RobotController::robotTaskAssigned);
    connect(m_manager, &RobotManager::robotTaskFinished,
            this, &RobotController::robotTaskFinished);
    connect(m_manager, &RobotManager::robotLowBattery,
            this, &RobotController::robotLowBattery);
    connect(m_manager, &RobotManager::robotFaultDetected,
            this, &RobotController::robotFaultDetected);
}

RobotController::~RobotController()
{
}

bool RobotController::addRobot(int id, const QString &ip)
{
    return m_manager->addRobot(id, ip);
}

bool RobotController::removeRobot(int id)
{
    return m_manager->removeRobot(id);
}

bool RobotController::updatePosition(int id, float x, float y)
{
    return m_manager->updateRobotPosition(id, x, y);
}

bool RobotController::updateBattery(int id, int battery)
{
    return m_manager->updateRobotBattery(id, battery);
}

bool RobotController::updateRobotIp(int id, const QString &ip)
{
    return m_manager->updateRobotIp(id, ip);
}

bool RobotController::updateStatus(int id, RobotStatus status)
{
    return m_manager->updateRobotStatus(id, status);
}

bool RobotController::assignTask(int id, int taskId)
{
    return m_manager->assignTaskToRobot(id, taskId);
}

bool RobotController::finishTask(int id)
{
    return m_manager->finishRobotTask(id);
}

bool RobotController::cancelTask(int id)
{
    return m_manager->cancelRobotTask(id);
}

QString RobotController::printAllRobots() const
{
    return m_manager->printAllRobots();
}

QList<int> RobotController::getOnlineRobots() const
{
    return m_manager->getOnlineRobots();
}

bool RobotController::isRobotExist(int id) const
{
    return m_manager->hasRobot(id);
}

const Robot *RobotController::getRobot(int id) const
{
    return m_manager->getRobot(id);
}

QList<int> RobotController::getAllRobotIds() const
{
    return m_manager->getAllRobotIds();
}

int RobotController::getRobotCount() const
{
    return m_manager->getRobotCount();
}

int RobotController::getIdleCount() const
{
    return m_manager->getIdleCount();
}

int RobotController::getBusyCount() const
{
    return m_manager->getBusyCount();
}

int RobotController::getFaultCount() const
{
    return m_manager->getFaultCount();
}