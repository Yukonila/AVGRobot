#ifndef ROBOT_H
#define ROBOT_H

#include <QString>

// 0-空闲；1-执行任务；2-故障；3-离线
enum class RobotStatus
{
    Idle = 0,
    Busy = 1,
    Error = 2,
    Offline = 3
};

class Robot
{
public:
    Robot();
    ~Robot(){};

// GET/SET
#pragma region
    int getId() const;
    void setId(int _id);

    float getPx() const;
    void setPx(float _x);
    float getPy() const;
    void setPy(float _y);

    int getBattery() const;
    void setBattery(int _battery);
    void increaseBattery(int _value = 1);
    void decreaseBattery(int _value = 1);

    RobotStatus getStatus() const;
    void setStatus(RobotStatus _status);
    float getSpeed() const;
    void setSpeed(int _speed);

    int getTask() const;
    void setTask(int _tId);

    QString getIp() const;
    void setIp(QString _ip);
#pragma endregion


private:
    int r_id;             // 机器人ID
    float r_x, r_y;       // 当前位置
    int r_battery;        // 电量0-100
    RobotStatus r_status; // 0-空闲；1-执行任务；2-故障；3-离线
    float r_curspeed;     // 当前速度
    int r_curTaskId;      // 执行的任务id -1为无任务
    QString r_ip;         // 网络地址
};

#endif
