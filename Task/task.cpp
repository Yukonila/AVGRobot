#include "task.h"

Task::Task()
    : m_taskId(-1), m_priority(0), m_startX(0.0f), m_startY(0.0f), m_endX(0.0f), m_endY(0.0f), m_assignedRobotId(-1), m_status(TaskStatus::Pending), m_createTime(QDateTime::currentDateTime())
{
}

Task::Task(int taskId, int priority, float startX, float startY, float endX, float endY, const QString &description)
{
    m_taskId = taskId;
    m_priority = priority;
    m_startX = startX;
    m_startY = startY;
    m_endX = endX;
    m_endY = endY;
    m_description = description;
    m_createTime = QDateTime::currentDateTime();
}

int Task::getTaskId() const
{
    return m_taskId;
}

int Task::getPriority() const
{
    return m_priority;
}

float Task::getStartX() const
{
    return m_startX;
}

float Task::getStartY() const
{
    return m_startY;
}

float Task::getEndX() const
{
    return m_endX;
}

float Task::getEndY() const
{
    return m_endY;
}

int Task::getAssignedRobotId() const
{
    return m_assignedRobotId;
}

TaskStatus Task::getStatus() const
{
    return m_status;
}

QDateTime Task::getCreateTime() const
{
    return m_createTime;
}

QDateTime Task::getStartTime() const
{
    return m_startTime;
}

QDateTime Task::getFinishTime() const
{
    return m_finishTime;
}

QString Task::getDescription() const
{
    return m_description;
}

QString Task::getStatusString() const
{
    switch (m_status)
    {
    case TaskStatus::Pending:
        return "待分配";
    case TaskStatus::Executing:
        return "执行中";
    case TaskStatus::Completed:
        return "已完成";
    case TaskStatus::Failed:
        return "异常";
    case TaskStatus::Cancelled:
        return "已取消";
    default:
        return "未知";
    }
}

void Task::setPriority(int priority)
{
    m_priority = priority;
}

void Task::setStartPoint(float x, float y)
{
    m_startX = x;
    m_startY = y;
}

void Task::setEndPoint(float x, float y)
{
    m_endX = x;
    m_endY = y;
}

void Task::setDescription(const QString &description)
{
    m_description = description;
}

void Task::setAssignedRobotId(int robotId)
{
    m_assignedRobotId = robotId;
}

void Task::setStatus(TaskStatus status)
{
    m_status = status;
}

void Task::markAsPending()
{
    m_status = TaskStatus::Pending;
    m_assignedRobotId = -1;
    m_startTime = QDateTime();
    m_finishTime = QDateTime();
}

void Task::markAsExecting()
{
    if (m_status == TaskStatus::Pending)
    {
        m_status = TaskStatus::Executing;
        m_startTime = QDateTime::currentDateTime();
    }
}

void Task::markAsCompleted()
{
    if (m_status == TaskStatus::Executing)
    {
        m_status = TaskStatus::Completed;
        m_finishTime = QDateTime::currentDateTime();
    }
}

void Task::markAsFailed()
{
    m_status = TaskStatus::Failed;
    m_finishTime = QDateTime::currentDateTime();
}

void Task::markAsCancelled()
{
    m_status = TaskStatus::Cancelled;
    m_finishTime = QDateTime::currentDateTime();
}

bool Task::isPending() const
{
    return m_status == TaskStatus::Pending;
}

bool Task::isExecuting() const
{
    return m_status == TaskStatus::Executing;
}

bool Task::isCompleted() const
{
    return m_status == TaskStatus::Completed;
}

bool Task::isFailed() const
{
    return m_status == TaskStatus::Failed;
}

bool Task::isCancelled() const
{
    return m_status == TaskStatus::Cancelled;
}

bool Task::isFinished() const
{
    return m_status == TaskStatus::Cancelled ||
           m_status == TaskStatus::Failed ||
           m_status == TaskStatus::Completed;
}

bool Task::isAssigned() const
{
    return m_assignedRobotId != -1;
}

QString Task::toString() const
{
    QString result;
    result += "=== 任务信息 ===\n";
    result += "任务ID: " + QString::number(m_taskId) + "\n";
    result += "优先级: " + QString::number(m_priority) + "\n";
    result += "起点: (" + QString::number(m_startX, 'f', 1) + ", " +
              QString::number(m_startY, 'f', 1) + ")\n";
    result += "终点: (" + QString::number(m_endX, 'f', 1) + ", " +
              QString::number(m_endY, 'f', 1) + ")\n";
    result += "分配机器人: " + QString::number(m_assignedRobotId) + "\n";
    result += "状态: " + getStatusString() + "\n";
    result += "创建时间: " + m_createTime.toString("yyyy-MM-dd hh:mm:ss") + "\n";
    if (m_startTime.isValid())
    {
        result += "开始时间: " + m_startTime.toString("yyyy-MM-dd hh:mm:ss") + "\n";
    }
    if (m_finishTime.isValid())
    {
        result += "完成时间: " + m_finishTime.toString("yyyy-MM-dd hh:mm:ss") + "\n";
    }
    if (!m_description.isEmpty())
    {
        result += "描述: " + m_description + "\n";
    }
    return result;
}

QString Task::toBriefString() const
{
    return QString("任务 #%1 | 起点(%2,%3) → 终点(%4,%5) | %6 | 机器人: %7")
        .arg(m_taskId)
        .arg(m_startX, 0, 'f', 1)
        .arg(m_startY, 0, 'f', 1)
        .arg(m_endX, 0, 'f', 1)
        .arg(m_endY, 0, 'f', 1)
        .arg(getStatusString())
        .arg(m_assignedRobotId == -1 ? "未分配" : QString::number(m_assignedRobotId));
}
