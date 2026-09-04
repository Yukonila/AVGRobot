#ifndef TASK_H
#define TASK_H

#include <QString>
#include <QDateTime>

enum class TaskStatus
{
    Pending = 0,   // 待分配
    Executing = 1, // 执行中
    Completed = 2, // 已完成
    Failed = 3,    // 异常/失败
    Cancelled = 4  // 已取消
};

class Task
{
public:
    Task();
    Task(int taskId, int priority, float startX, float startY, float endX, float endY, const QString &description = "");
    ~Task() = default;

    //====geter====
    int getTaskId() const;
    int getPriority() const;
    float getStartX() const;
    float getStartY() const;
    float getEndX() const;
    float getEndY() const;
    int getAssignedRobotId() const;
    TaskStatus getStatus() const;
    QDateTime getCreateTime() const;
    QDateTime getStartTime() const;
    QDateTime getFinishTime() const;
    QString getDescription() const;
    QString getStatusString() const;

    void setPriority(int priority);
    void setStartPoint(float x, float y);
    void setEndPoint(float x, float y);
    void setDescription(const QString &description);
    void setAssignedRobotId(int robotId);

    void setStatus(TaskStatus status);
    void markAsPending();
    void markAsExecting();
    void markAsCompleted();
    void markAsFailed();
    void markAsCancelled();

    bool isPending() const;
    bool isExecuting() const;
    bool isCompleted() const;
    bool isFailed() const;
    bool isCancelled() const;
    bool isFinished() const;
    bool isAssigned() const;

    // ========== 工具方法 ==========
    QString toString() const;
    QString toBriefString() const;

private:
    int m_taskId;           // 任务ID
    int m_priority;         // 优先级: 0-低, 1-中, 2-高
    float m_startX;         // 起点 X
    float m_startY;         // 起点 Y
    float m_endX;           // 终点 X
    float m_endY;           // 终点 Y
    int m_assignedRobotId;  // 分配的机器人ID, -1表示未分配
    TaskStatus m_status;    // 任务状态
    QDateTime m_createTime; // 创建时间
    QDateTime m_startTime;  // 开始执行时间
    QDateTime m_finishTime; // 完成时间
    QString m_description;  // 任务描述
};

#endif // TASK_H
