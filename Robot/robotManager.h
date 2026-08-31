#ifndef ROBOTMANAGER_H
#define ROBOTMANAGER_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QMutex>
#include "robot.h"

class RobotManager : public QObject
{
    Q_OBJECT
public:
    explicit RobotManager(QObject *parent = nullptr);
    ~RobotManager();
    // 添加机器人函数
    bool addRobot(const Robot &_bot);
    bool addRobot(int id, const QString &ip = QString(""));
    // 删除机器人函数
    bool removeRobot(int id);
    void clearRobot();

signals:
    void robotAdded(int id);
    void robotRemoved(int id);
    // debug调试0  || INFO信息 1  ||ERROR错误 2 || Warning警告 3
    void logMessage(const QString &msg, int level);

private:
    QMap<int, Robot> m_robot; // 储存机器人的数据结构
    mutable QMutex m_mutex;   // 锁
};

#endif // ROBOTMANAGER_H
