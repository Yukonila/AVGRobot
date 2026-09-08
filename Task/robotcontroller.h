#ifndef ROBOTCONTROLLER_H
#define ROBOTCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QVector>
#include "robotmanager.h"
#include "taskmanager.h"
#include "taskscheduler.h"

class RobotController : public QObject
{
    Q_OBJECT

public:
    explicit RobotController(QObject *parent = nullptr);
    ~RobotController();

    // ========== 机器人管理 ==========
    bool addRobot(int id, const QString &ip = "127.0.0.1");
    bool removeRobot(int id);
    bool updatePosition(int id, float x, float y);
    bool updateBattery(int id, int battery);
    bool updateSpeed(int id, float speed);
    bool updateRobotAccel(int id, float accel);
    bool updateRobotMaxLoad(int id, int maxLoad);
    bool updateStatus(int id, RobotStatus status);
    const Robot *getRobot(int id) const;
    Robot *getRobot(int id);
    QList<int> getAllRobotIds() const;
    int getRobotCount() const;
    int getIdleCount() const;
    int getBusyCount() const;
    int getFaultCount() const;
    bool isRobotBusy(int id) const;
    bool updateRobotIp(int id, const QString &ip);
    // 生成一个"充电桩为圆心、圆内随机"的出生点(避障、不出界)
    QPointF nextSpawnPos() const;
    // 对头相撞卡住时，把占道/低优先级的一方横向让开一格，破除僵局
    void resolveConflicts();

    // ========== 任务管理 ==========
    bool addTask(int taskId, int priority, float startX, float startY,
                 float endX, float endY, const QString &desc = "");
    bool addTask(const Task &task);
    bool removeTask(int taskId);
    bool removeNewestTask(); // 删除最近创建的任务
    void clearAllTasks();    // 清空全部任务
    void removeAllRobots();  // 清空全部机器人(尽量释放)
    Task *getTask(int taskId);
    // 取消/回收：同时释放绑定的机器人
    bool cancelExecutingTask(int taskId);
    bool recycleTask(int taskId);
    QList<int> getAllTaskIds() const;
    QList<int> getPendingTaskIds() const;
    QList<int> getExecutingTaskIds() const;
    int getPendingCount() const;
    int getExecutingCount() const;
    int getCompletedCount() const;
    int getFailedCount() const;
    int getCancelledCount() const;

    // ========== 调度控制 ==========
    void startScheduler(int intervalMs = 3000);
    void stopScheduler();
    void scheduleOnce();
    bool isSchedulerRunning() const;
    void setEnableReturnHome(bool enable);   // 自动回原点开关(空闲即回)
    bool isReturnHomeEnabled() const;
    bool returnIdleRobotsToHome();           // 手动“全部(空闲)回原点”，不打断执行中任务
    void setSimulationMode(bool simulate);   // true=模拟移动; false=由TCP等真实数据更新位置
    bool isSimulationMode() const;

    // ========== 数据持久化(DataManager) ==========
    bool saveData(const QString &filePath = "");
    bool loadData(const QString &filePath = "");
    bool loadRobotsOnly();   // 仅从数据文件载入机器人(不含任务/地图)

    // ========== 地图网格 / 可达性(为 A*、避障铺路) ==========
    void setMapGrid(int cols, int rows, const QVector<char> &obstacles); // 1格=1世界单位
    bool isBlockedWorld(float x, float y) const;
    // 判断两个世界点(格子中心)是否可达(避开障碍的连通性)
    bool isReachable(float ax, float ay, float bx, float by) const;
    // 返回两点间的网格路径(世界坐标、格子中心)，空=不可达；用于避障路径规划/绘制
    QList<QPointF> planPathWorld(float ax, float ay, float bx, float by) const;

    // ========== 电量 / 充电桩 (模拟) ==========
    float maxSpeed() const;          // 机器人速度上限
    float lowChargeLevel() const;    // 低于该电量去充电(%)
    QList<QPointF> chargers() const; // 充电桩列表
    void addCharger(float x, float y);
    QPointF nearestCharger(float x, float y) const;
    void clearChargers();
    void addDefaultCharger();        // 恢复默认原点充电桩

    // ========== 调试 ==========
    QString printAllRobots() const;
    QString printAllTasks() const;

signals:
    // 日志信号
    void logMessage(const QString &msg, int level);

    // 机器人信号（透传）
    void robotAdded(int robotId);
    void robotRemoved(int robotId);
    void robotStatusChanged(int robotId, RobotStatus oldStatus, RobotStatus newStatus);
    void robotPositionChanged(int robotId, float x, float y);
    void robotBatteryChanged(int robotId, int battery);

    // 任务信号（透传）
    void taskAdded(int taskId);
    void taskRemoved(int taskId);
    void taskAssigned(int taskId, int robotId);
    void taskFinished(int taskId);
    void taskFailed(int taskId);
    void taskCancelled(int taskId);

    // 调度信号
    void schedulerStarted();
    void schedulerStopped();
    void robotReturnedHome(int robotId);

private:
    RobotManager *m_robotManager;
    TaskManager *m_taskManager;
    TaskScheduler *m_scheduler;

    // 移动模拟（调度运行期间，让机器人逐段移动：任务起点→任务终点→回原点）
    QTimer *m_simTimer;
    void stepRobots(); // 每拍推进机器人位置
    void stepBusyRobot(int id, Robot *r, float dt); // 执行中：沿避障路径走
    bool stepToward(int id, float tx, float ty, float speed, float dt); // 沿避障路径向目标走一步(仍在途中=真)
    bool robotProximityBlocked(int id, float nx, float ny) const; // 距其它机器人太近则停
    int m_simIntervalMs;
    bool m_isReturnHome;    // 自动回原点开关(默认开)
    bool m_homeIdleNow;     // “全部回原点”一次性触发标记
    bool m_simulateMovement;// 模拟模式=true; TCP接入后由真实数据更新位置=false

    // 每台机器人“当前任务→是否已到达过任务起点(锁定下一步去终点)”
    QHash<int, int> m_robotTaskPhase;   // robotId -> taskId
    QHash<int, bool> m_robotStartDone;  // robotId -> 已到过起点

    // 执行中机器人的避障路径(格子中心世界坐标)
    QHash<int, QList<QPointF>> m_robotPlan;  // robotId -> 路径
    QHash<int, int> m_robotPlanTask;         // robotId -> 该路径对应的 taskId
    QHash<int, int> m_robotPlanIdx;          // robotId -> 当前走到第几个点

    // 回原点阶段日志标记：0=未提示,1=已提示返回中,2=已提示回到
    QHash<int, int> m_robotReturnStage;

    // 电量/充电
    QList<QPointF> m_chargers;               // 充电桩位置(默认含原点)
    QHash<int, QPointF> m_chargeTarget;      // robotId -> 目标充电桩
    float m_maxRobotSpeed;
    float m_lowChargeLevel;                  // 去充电的电量阈值
    float m_chargePerSec;                    // 充电速度(电量%/秒)
    float m_drainPerUnit;                    // 每单位距离掉电系数
    void drainBattery(int robotId, float distance, float speed);
    bool goChargeIfLow(int robotId);         // 低电空闲→去充电，返回是否转入充电

    // 地图网格(避障/可达性)
    int m_gridCols = 50;
    int m_gridRows = 30;
    QVector<char> m_obstacles;               // 0 空闲 / 1 障碍
    int cellIndex(int cx, int cy) const { return cy * m_gridCols + cx; }
    // 家的位置：放在画布中间某格子的中心(充电桩位于格内，不压网格线)
    QPointF homePoint() const
    {
        return QPointF((int)(m_gridCols * 0.5f) + 0.5f,
                       (int)(m_gridRows * 0.5f) + 0.5f);
    }
    // 该世界坐标是否落在某充电桩的格内(非本机器人目标桩则不可进入)
    bool blockedByCharger(int robotId, float wx, float wy) const;
};

#endif // ROBOTCONTROLLER_H