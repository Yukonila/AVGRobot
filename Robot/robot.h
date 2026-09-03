#ifndef ROBOT_H
#define ROBOT_H

#include <QString>
#include <QDateTime>
#include <QHostAddress>

// 机器人状态(与项目文档 5.2「机器人管理」/状态监控一致)
enum class RobotStatus
{
    None = -1,     // 未知/无效
    Idle = 0,      // 0-空闲(可接任务)
    Busy = 1,      // 1-执行任务(忙碌)
    Error = 2,     // 2-故障
    Offline = 3,   // 3-离线
    Lowbattery = 4,// 4-低电量
    Charging = 5   // 5-充电中

};

class Robot
{
public:
    Robot();
    ~Robot() {};

#pragma region get/set
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
    void setSpeed(float _speed);

    int getTask() const;
    void setTask(int _tId);

    QString getIp() const;
    void setIp(QString _ip);
    void setAll(int id, float x, float y, int battery, float speed, int taskid, RobotStatus status, QString ip);

#pragma endregion

    bool isAvailable();
    QString printRobot() const;
    QString getErrorMsg() const;
    static bool isValidIp(const QString &ipAddress);

private:
    int r_id;             // 机器人ID
    float r_x, r_y;       // 当前位置
    int r_battery;        // 电量0-100
    RobotStatus r_status; // 机器人状态：0-空闲 1-执行任务 2-故障 3-离线 4-低电量 5-充电
    float r_curspeed;     // 当前速度
    int r_curTaskId;      // 执行的任务id -1为无任务
    QString r_errorMsg;
    QString r_ip;          // 网络地址
    QDateTime r_creatTime; // 创建时间
};

#endif
