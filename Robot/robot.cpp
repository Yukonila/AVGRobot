#include "robot.h"

Robot::Robot()
    : r_id(0), r_x(0.0f), r_y(0.0f), r_battery(100), r_creatTime(QDateTime::currentDateTime()),
      r_status(RobotStatus::Idle), r_curspeed(0.0f), r_ip(""), r_curTaskId(-1), r_errorMsg("")
{
}

#pragma region get/set

int Robot::getId() const
{
    return r_id;
}

void Robot::setId(int _id)
{
    r_id = _id;
}

float Robot::getPx() const
{
    return r_x;
}

void Robot::setPx(float _x)
{
    r_x = _x;
}

float Robot::getPy() const
{
    return r_y;
}

void Robot::setPy(float _y)
{
    r_y = _y;
}

int Robot::getBattery() const
{
    return r_battery;
}

void Robot::setBattery(int _battery)
{
    if (_battery < 0 || _battery > 100)
    {
        r_errorMsg = "电量范围设置错误";
        return;
    }
    r_battery = _battery;
}

void Robot::increaseBattery(int _value)
{
    if (r_battery + _value > 100 || _value < 0)
    {
        r_errorMsg = "电量增量超量程！";
        return;
    }
    r_battery += _value;
}

void Robot::decreaseBattery(int _value)
{
    if (r_battery - _value < 0 || _value < 0)
    {
        r_errorMsg = "电量减少量超量程！";
        return;
    }
    r_battery -= _value;
}

RobotStatus Robot::getStatus() const
{
    return r_status;
}

void Robot::setStatus(RobotStatus _status)
{
    r_status = _status;
}

float Robot::getSpeed() const
{
    return r_curspeed;
}

void Robot::setSpeed(float _speed)
{
    if (_speed < 0)
    {
        r_errorMsg = "速度值小于最低值！";
        return;
    }
    r_curspeed = _speed;
}

int Robot::getTask() const
{
    return r_curTaskId;
}

void Robot::setTask(int _tId)
{
    r_curTaskId = _tId;
}

QString Robot::getIp() const
{
    return r_ip;
}

void Robot::setIp(QString _ip)
{
    r_ip = _ip;
}

#pragma endregion

// 是否空闲
bool Robot::isAvailable()
{
    return r_status == RobotStatus::Idle;
}
// 打印信息
QString Robot::printRobot()
{
    QString msg;
    msg += "机器人信息：\n";
    msg += "ID: " + QString::number(r_id) + "\n";
    msg += "位置: (" + QString::number(r_x) + ", " + QString::number(r_y) + ")\n";
    msg += "电量: " + QString::number(r_battery) + "%\n";
    msg += "状态: " + QString::number(static_cast<int>(r_status)) + "\n";
    msg += "速度: " + QString::number(r_curspeed) + "\n";
    msg += "IP: " + r_ip + "\n";
    msg += "创建时间: " + r_creatTime.toString("yyyy-MM-dd hh:mm:ss");
    msg += "任务ID: " + QString::number(r_curTaskId);
    return msg;
}

QString Robot::getErrorMsg()
{
    return r_errorMsg;
}
