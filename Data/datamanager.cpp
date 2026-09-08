#include "datamanager.h"

// ========== 构造/析构 ==========

DataManager::DataManager(QObject *parent)
    : QObject(parent), m_filePath(getDefaultFilePath())
{
}

DataManager::~DataManager()
{
}

// ========== 文件路径 ==========

QString DataManager::getDefaultFilePath() const
{
    // 在可执行文件同目录下创建 Data 文件夹
    QDir dir = QDir::current();
    if (!dir.exists("Data"))
    {
        dir.mkdir("Data");
    }
    return dir.absolutePath() + "/Data/data.json";
}

QString DataManager::getCurrentFilePath() const
{
    return m_filePath;
}

bool DataManager::isFileExists() const
{
    return QFile::exists(m_filePath);
}

// ========== 文件读写 ==========

bool DataManager::loadFromFile(const QString &filePath)
{
    if (!filePath.isEmpty())
    {
        m_filePath = filePath;
    }

    if (!isFileExists())
    {
        emit logMessage("[DataManager] 数据文件不存在，使用默认数据", 1);
        return false;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        emit logMessage("[DataManager] 无法打开数据文件: " + m_filePath, 2);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
    {
        emit logMessage("[DataManager] 数据文件格式错误", 2);
        return false;
    }

    emit logMessage("[DataManager] 数据加载成功: " + m_filePath, 0);
    return true;
}

bool DataManager::saveToFile(const QString &filePath)
{
    if (!filePath.isEmpty())
    {
        m_filePath = filePath;
    }

    // 确保目录存在
    QFileInfo fileInfo(m_filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    // 这里只是保存空对象，实际保存由 saveAllData 完成
    // 此方法保留用于未来扩展
    return true;
}

// ========== 完整数据读写 ==========

bool DataManager::saveAllData(const QList<Robot> &robots,
                              const QList<Task> &tasks,
                              int tcpPort,
                              int schedulerInterval,
                              bool enableReturnHome)
{
    QJsonObject root;
    root["robots"] = robotsToJson(robots);
    root["tasks"] = tasksToJson(tasks);

    
    QJsonObject config;
    config["tcpPort"] = tcpPort;
    config["schedulerInterval"] = schedulerInterval;
    config["enableReturnHome"] = enableReturnHome;
    config["lowBatteryThreshold"] = 20;
    root["config"] = config;

    // 写入文件
    QJsonDocument doc(root);
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        emit logMessage("[DataManager] 无法写入数据文件: " + m_filePath, 2);
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    emit logMessage("[DataManager] 数据保存成功: " + m_filePath, 0);
    return true;
}

bool DataManager::loadAllData(QList<Robot> &outRobots,
                              QList<Task> &outTasks,
                              int &outTcpPort,
                              int &outSchedulerInterval,
                              bool &outEnableReturnHome)
{
    outTcpPort = 8888;
    outSchedulerInterval = 3000;
    outEnableReturnHome = true;

    if (!loadFromFile())
    {
        return false;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
    {
        return false;
    }

    QJsonObject root = doc.object();

    // 加载机器人
    if (root.contains("robots") && root["robots"].isArray())
    {
        outRobots = robotsFromJson(root["robots"].toArray());
        emit logMessage("[DataManager] 加载了 " + QString::number(outRobots.size()) + " 个机器人", 0);
    }

    // 加载任务
    if (root.contains("tasks") && root["tasks"].isArray())
    {
        outTasks = tasksFromJson(root["tasks"].toArray());
        emit logMessage("[DataManager] 加载了 " + QString::number(outTasks.size()) + " 个任务", 0);
    }

    // 加载配置
    if (root.contains("config") && root["config"].isObject())
    {
        QJsonObject config = root["config"].toObject();
        outTcpPort = config.value("tcpPort").toInt(8888);
        outSchedulerInterval = config.value("schedulerInterval").toInt(3000);
        outEnableReturnHome = config.value("enableReturnHome").toBool(true);
        emit logMessage("[DataManager] 配置加载成功: 端口=" + QString::number(outTcpPort) +
                            ", 调度间隔=" + QString::number(outSchedulerInterval) + "ms" +
                            ", 自动回原点=" + (outEnableReturnHome ? "开" : "关"),
                        0);
    }

    return true;
}

// ========== 机器人序列化 ==========

QJsonArray DataManager::robotsToJson(const QList<Robot> &robots) const
{
    QJsonArray array;
    for (const Robot &robot : robots)
    {
        array.append(toJsonObject(robot));
    }
    return array;
}

QList<Robot> DataManager::robotsFromJson(const QJsonArray &jsonArray) const
{
    QList<Robot> robots;
    for (const QJsonValue &value : jsonArray)
    {
        if (value.isObject())
        {
            robots.append(toRobot(value.toObject()));
        }
    }
    return robots;
}

QJsonObject DataManager::toJsonObject(const Robot &robot) const
{
    QJsonObject obj;
    obj["id"] = robot.getId();
    obj["x"] = robot.getPx();
    obj["y"] = robot.getPy();
    obj["battery"] = robot.getBattery();
    obj["speed"] = robot.getSpeed();
    obj["status"] = static_cast<int>(robot.getStatus());
    obj["taskId"] = robot.getTask();
    obj["ip"] = robot.getIp();
    return obj;
}

Robot DataManager::toRobot(const QJsonObject &json) const
{
    Robot robot;
    robot.setId(json.value("id").toInt());
    robot.setPx(json.value("x").toDouble());
    robot.setPy(json.value("y").toDouble());
    robot.setBattery(json.value("battery").toInt(100));
    robot.setSpeed(json.value("speed").toDouble(0.0));
    robot.setStatus(static_cast<RobotStatus>(json.value("status").toInt(0)));
    robot.setTask(json.value("taskId").toInt(-1));
    robot.setIp(json.value("ip").toString(""));
    return robot;
}

// ========== 任务序列化 ==========

QJsonArray DataManager::tasksToJson(const QList<Task> &tasks) const
{
    QJsonArray array;
    for (const Task &task : tasks)
    {
        array.append(toJsonObject(task));
    }
    return array;
}

QList<Task> DataManager::tasksFromJson(const QJsonArray &jsonArray) const
{
    QList<Task> tasks;
    for (const QJsonValue &value : jsonArray)
    {
        if (value.isObject())
        {
            tasks.append(toTask(value.toObject()));
        }
    }
    return tasks;
}

QJsonObject DataManager::toJsonObject(const Task &task) const
{
    QJsonObject obj;
    obj["taskId"] = task.getTaskId();
    obj["priority"] = task.getPriority();
    obj["startX"] = task.getStartX();
    obj["startY"] = task.getStartY();
    obj["endX"] = task.getEndX();
    obj["endY"] = task.getEndY();
    obj["assignedRobotId"] = task.getAssignedRobotId();
    obj["status"] = static_cast<int>(task.getStatus());
    obj["description"] = task.getDescription();
    obj["createTime"] = task.getCreateTime().toString(Qt::ISODate);

    if (task.getStartTime().isValid())
    {
        obj["startTime"] = task.getStartTime().toString(Qt::ISODate);
    }
    if (task.getFinishTime().isValid())
    {
        obj["finishTime"] = task.getFinishTime().toString(Qt::ISODate);
    }

    return obj;
}

Task DataManager::toTask(const QJsonObject &json) const
{
    int taskId = json.value("taskId").toInt();
    int priority = json.value("priority").toInt(0);
    float startX = json.value("startX").toDouble(0.0);
    float startY = json.value("startY").toDouble(0.0);
    float endX = json.value("endX").toDouble(0.0);
    float endY = json.value("endY").toDouble(0.0);
    QString description = json.value("description").toString("");

    Task task(taskId, priority, startX, startY, endX, endY, description);

    task.setAssignedRobotId(json.value("assignedRobotId").toInt(-1));
    task.setStatus(static_cast<TaskStatus>(json.value("status").toInt(0)));

    // ✅ 恢复创建时间
    QString timeStr = json.value("createTime").toString();
    if (!timeStr.isEmpty())
    {
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (dt.isValid())
        {
            task.setCreateTime(dt);
        }
    }

    // 恢复开始时间和完成时间（可选）
    QString startTimeStr = json.value("startTime").toString();
    if (!startTimeStr.isEmpty())
    {
        QDateTime dt = QDateTime::fromString(startTimeStr, Qt::ISODate);
        if (dt.isValid())
        {
            task.setStartTime(dt);
        }
    }

    QString finishTimeStr = json.value("finishTime").toString();
    if (!finishTimeStr.isEmpty())
    {
        QDateTime dt = QDateTime::fromString(finishTimeStr, Qt::ISODate);
        if (dt.isValid())
        {
            task.setFinishTime(dt);
        }
    }

    return task;
}

// ========== 配置序列化 ==========

QJsonObject DataManager::configToJson() const
{
    QJsonObject config;
    // 默认值，实际由 saveAllData 覆盖
    config["tcpPort"] = 8888;
    config["schedulerInterval"] = 3000;
    config["enableReturnHome"] = true;
    config["lowBatteryThreshold"] = 20;
    return config;
}

void DataManager::configFromJson(const QJsonObject &jsonObject)
{
    // 配置由调用方直接读取，这里只做验证
    Q_UNUSED(jsonObject);
    // 实际读取在 loadAllData 中完成
}