#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QList>
#include "robot.h"
#include "Task/task.h"

class DataManager : public QObject
{
    Q_OBJECT

public:
    explicit DataManager(QObject *parent = nullptr);
    ~DataManager();

    // ========== 文件操作 ==========
    bool loadFromFile(const QString &filePath = "");
    bool saveToFile(const QString &filePath = "");
    bool isFileExists() const;
    QString getCurrentFilePath() const;

    // ========== 机器人数据转换 ==========
    QJsonArray robotsToJson(const QList<Robot> &robots) const;
    QList<Robot> robotsFromJson(const QJsonArray &jsonArray) const;

    // ========== 任务数据转换 ==========
    QJsonArray tasksToJson(const QList<Task> &tasks) const;
    QList<Task> tasksFromJson(const QJsonArray &jsonArray) const;

    // ========== 系统配置 ==========
    QJsonObject configToJson() const;
    void configFromJson(const QJsonObject &jsonObject);

    // ========== 完整数据读写 ==========
    bool saveAllData(const QList<Robot> &robots,
                     const QList<Task> &tasks,
                     int tcpPort,
                     int schedulerInterval);

    bool loadAllData(QList<Robot> &outRobots,
                     QList<Task> &outTasks,
                     int &outTcpPort,
                     int &outSchedulerInterval);

signals:
    void logMessage(const QString &msg, int level);

private:
    // ========== 辅助方法 ==========
    QString getDefaultFilePath() const;
    QJsonObject toJsonObject(const Robot &robot) const;
    Robot toRobot(const QJsonObject &jsonObject) const;
    QJsonObject toJsonObject(const Task &task) const;
    Task toTask(const QJsonObject &jsonObject) const;

    QString m_filePath;
};

#endif // DATAMANAGER_H