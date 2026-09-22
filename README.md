# AVG 物流机器人任务调度系统（RCS）

> 基于 **C++17 + Qt 5** 的多机器人任务调度与监控上位机（Robot Control System）。
> 实现机器人接入与状态监控、任务创建与生命周期管理、优先级调度、网格避障路径规划、
> 电量/充电桩模拟、地图可视化编辑、TCP 真实机器人接入，以及账号鉴权与数据持久化。

本文档是**项目实现文档**：既说明技术栈与实现方法，也完整记录了开发过程中
**发现的问题、根因分析与解决方式**，并列出与产品文档（`02-AVG物流机器人任务调度系统.docx`）的差距。

---

## 1. 项目概述

### 1.1 定位

系统定位为**工业物流机器人集群的统一调度与管控中枢**，是上层业务系统（WMS/MES）与
底层机器人执行层之间的桥梁。核心能力：

| 能力 | 说明 |
| --- | --- |
| 多机器人管理 | 统一管理 ID / 位置 / 电量 / 速度 / 加速度 / 负载 / 状态 / 当前任务 |
| 任务全生命周期 | 待分配 → 执行中 → 已完成 / 异常 / 已取消，支持回收与重分配 |
| 智能调度 | 最小负载优先 + 最短距离优先，支持优先级抢占与超时回收 |
| 路径规划与避障 | 网格地图 + BFS/A* 最短路 + 动态障碍（其它机器人）重规划 |
| 交通与冲突处理 | 碰撞体积、接近即停、限频重规划绕行、让道解卡 |
| 电量与充电 | 按距离/速度掉电、低电自动回桩、充电桩格互斥占用 |
| 通信 | TCP 服务端，JSON 行协议 + 自定义二进制帧，心跳保活与离线判定 |
| 可视化 | 自绘网格画布 + 机器人/任务/路径叠加 + 右侧列表与详情面板 |
| 持久化与鉴权 | JSON 存档（机器人/任务/配置/地图/账号），管理员与普通用户双角色 |

### 1.2 一次完整的业务闭环

```
登录/注册 → 选择运行模式(模拟 / TCP)
   ↓
画布上设计地图(画障碍 / 擦除 / 放充电桩)      ← 未画地图前右侧面板与调度被门控禁用
   ↓
工具切到「新建任务」→ 点两点定起点/终点 + 选优先级(1~4)
   ↓ (自动做可达性 BFS 预检，不可达直接拒绝)
任务进入 pending 队列 → 调度器(1s/拍)分配最优机器人
   ↓
机器人沿避障路径 起点→终点，途中掉电、被挡则重规划
   ↓
到达终点(距离<0.5) → 任务完成 → 机器人转为空闲 → 低电则自动回桩充电
   ↓
全程日志/表格/画布实时刷新；可随时停止、强制停止机器人、回收任务
```

---

## 2. 技术栈

| 层次 | 技术选型 |
| --- | --- |
| 语言标准 | **C++17**（`CONFIG += c++17`，使用 `enum class`、lambda、`std::sort/stable_sort`、结构化初始化） |
| GUI 框架 | **Qt 5.15 Widgets**（`QT += widgets network`），`.ui` 设计器 + 纯代码布局混用 |
| 图形绘制 | `QPainter` 自绘网格画布（格子、障碍、路径、机器人、充电桩、起终点标记） |
| 事件模型 | **Qt 信号槽**（`QObject::connect`，C++11 函数指针 + lambda 语法） |
| 定时驱动 | `QTimer`：调度拍(1000ms)、移动模拟(250ms)、TCP 心跳(1000ms) |
| 并发安全 | `QMutex` + `QMutexLocker`（非递归锁，**锁内不发业务信号**） |
| 网络通信 | `QTcpServer` / `QTcpSocket`（服务端），自定义二进制帧 + JSON 行协议 |
| 数据持久化 | `QJsonDocument` / `QJsonObject` / `QJsonArray` 读写 JSON 文件 |
| 构建系统 | **qmake**（`AVGRobot.pro`）+ make，**影子构建**目录 `build/vscode` |
| 开发环境 | VS Code（`tasks.json` / `launch.json` / `c_cpp_properties.json`）/ Qt Creator |
| 附带工具 | `tools/robotClient`：模拟真实机器人的控制台 TCP 客户端 |

---

## 3. 目录结构与模块划分

按**职责分层 + 高内聚**重构后的目录（`mv` 前的散落文件已归入模块目录）：

```
AVGRobot/
├── AVGRobot.pro              # qmake 工程(含各模块 INCLUDEPATH)
├── README.md                 # 本文档
├── app/                      # 应用与界面层
│   ├── main.cpp              # 入口：仅启动登录窗口
│   ├── loginwindow.{h,cpp}   # 登录/注册 + 运行模式选择
│   ├── mainwindow.{h,cpp,ui} # 主窗口：菜单、右侧面板、日志、信号汇聚
├── view/
│   └── mapeditor.{h,cpp}     # 合一画布：地图编辑 + 监控叠加 + 画布式新建任务
├── Dialog/
│   └── robotdialog.{h,cpp,ui}# 机器人新增/编辑对话框
├── Robot/                    # 机器人领域层
│   ├── robot.{h,cpp}         # Robot 实体 + RobotStatus 枚举 + IP 校验
│   └── robotmanager.{h,cpp}  # 机器人集合管理(线程安全) + 状态查询/统计
├── Task/                     # 任务领域层 + 业务编排层
│   ├── task.{h,cpp}          # Task 实体 + TaskStatus 状态机
│   ├── taskmanager.{h,cpp}   # 任务集合 + 按状态的索引列表
│   ├── taskscheduler.{h,cpp} # 调度引擎(LLF+SDF、抢占、超时回收)
│   └── robotcontroller.{h,cpp}# ★聚合门面：机器人+任务+调度+移动模拟+充电+持久化
├── Tcp/
│   └── tcpRobotServer.{h,cpp}# TCP 服务端：注册、上报解析、心跳、任务下发
├── Data/
│   ├── datamanager.{h,cpp}   # 机器人/任务/配置的 JSON 序列化
│   ├── usermanager.{h,cpp}   # 账号库(JSON)与角色校验
│   ├── users.json            # 账号数据
│   └── map.json / data.json  # 运行时生成的地图与业务存档
└── tools/robotClient/        # 独立的机器人模拟器(可执行程序)
    ├── robotClient.pro
    └── main.cpp
```

**设计要点：`RobotController` 作为唯一聚合门面。**
UI 层只持有 `RobotController*`，不直接接触 `RobotManager` / `TaskManager` / `TaskScheduler`。
好处：① 跨模块的一致性操作（如"删任务必须同时释放机器人"）只在一处实现；
② UI 与业务解耦，TCP 模式/模拟模式只是控制器的一个开关。

| 模块 | 代码量 | 职责 |
| --- | ---: | --- |
| `app/mainwindow.cpp` | ~1100 行 | UI 编排、菜单、表格刷新、TCP 接线 |
| `view/mapeditor.cpp` | ~880 行 | 画布渲染与交互 |
| `Task/robotcontroller.cpp` | ~1176 行 | 业务聚合与运动模拟 |
| `Robot/robotmanager.cpp` | ~750 行 | 机器人群管理 |
| `Task/taskmanager.cpp` | ~660 行 | 任务群管理 |
| `Data/datamanager.cpp` | ~370 行 | 序列化 |
| `Task/taskscheduler.cpp` | ~352 行 | 调度算法 |

---

## 4. 系统架构与数据流

```
┌────────────────────────── 应用与界面层 (app/ view/ Dialog/) ──────────────────────────┐
│  LoginWindow ──登录成功──▶ MainWindow ──持有──▶ MapEditorWidget(画布)                 │
│       ▲                        │  信号槽(日志/表格/状态栏/详情面板)                    │
│       └────logoutRequested─────┘                                                      │
└────────────────────────────────────┬──────────────────────────────────────────────────┘
                                     │ 只依赖 RobotController（门面）
┌────────────────────────────────────▼──────────────────────────────────────────────────┐
│                          业务编排层  Task/RobotController                             │
│  移动模拟(250ms) │ 电量/充电模拟 │ 路径规划与重规划 │ 持久化 │ 冲突处理 │ 门控 API     │
└──────┬──────────────────────┬───────────────────────┬─────────────────────────────────┘
       │                      │                       │
┌──────▼──────┐      ┌────────▼────────┐     ┌────────▼─────────┐
│ RobotManager│◀────▶│  TaskManager    │◀───▶│  TaskScheduler   │
│ QMap<int,   │      │  pending/exec/  │     │ LLF+SDF/抢占/超时 │
│ Robot>+Mutex│      │  done/fail/cancel│    │ QTimer 1s 拍      │
└─────────────┘      └─────────────────┘     └──────────────────┘
       ▲
       │ robotReported / robotConnected / robotHeartbeatTimeout
┌──────┴────────────────────────────────────────────────────────────────────────────────┐
│                        通信层 TcpRobotServer (QTcpServer)                              │
│   JSON 行协议(注册/上报)  +  二进制帧[AA55][len][func][data][xor][DDEE]               │
└────────────────────────────────────┬──────────────────────────────────────────────────┘
                                     │ TCP :8888
                        ┌────────────▼────────────┐
                        │ tools/robotClient 或真机 │
                        └─────────────────────────┘
```

**数据流向（上行）**：机器人 → TCP 帧 → 解析 JSON → `robotReported` 信号 →
`MainWindow` 转调 `RobotController::updatePosition/updateBattery/updateStatus` →
`RobotManager` 发细粒度信号 → 表格/画布/详情刷新。

**数据流向（下行）**：调度器 `assignTask` → `RobotController::taskAssigned` →
`MainWindow` 组装 `QJsonObject{func:2,...}` → `TcpRobotServer::sendToRobot` →
`0x02` 二进制帧 → 机器人。

---

## 5. 核心模块实现方法

### 5.1 Robot / RobotManager（机器人管理）

- **实体**：`Robot` 封装 `id / (x,y) / battery / status / speed / accel / maxLoad / load / curTaskId / ip / creatTime`。
  所有 setter 带**范围校验**（如 `setBattery` 限 0~100，越界写入 `r_errorMsg` 并拒绝）。
- **状态枚举**与产品文档 5.2 保持一致：`Idle=0 / Busy=1 / Error=2 / Offline=3 / Lowbattery=4 / Charging=5`，
  另加 `None=-1` 表示未知，便于 TCP 上报非法值时的兜底。
- **存储**：`QMap<int, Robot>`（值语义，避免 `QMap<int, Robot*>` 的裸指针泄漏与悬垂）。
- **线程安全**：所有读写都在 `QMutexLocker` 保护下完成；**锁内不发业务信号**，
  先解锁再 `emit robotAdded/robotStatusChanged/...`，避免槽函数回调用加锁接口造成**自死锁**。
- **一致性保证**：`assignTaskToRobot` 同时更新 `robot.curTaskId` 与 `status=Busy`；
  `finishRobotTask / cancelRobotTask` 统一置 `taskId=-1` 且状态回 `Idle`，保证"机器人忙碌但无 taskId"不会再出现。
- **快捷查询**：`getIdleRobots / getBusyRobots / getLowBatteryRobots(threshold) / getRobotsSortedByLoad`。

### 5.2 Task / TaskManager（任务管理）

- **状态机**：`Pending → Executing → Completed | Failed | Cancelled`，`isFinished()` 统一判定终态。
- **关键修复**：7 参数构造函数的初始化列表必须显式写 `m_status(TaskStatus::Pending)`、`m_assignedRobotId(-1)`、
  三个时间字段为空——否则状态是未初始化随机值，任务**永远不会进入 pending 队列**。
- **索引结构**（避免每次全表扫描）：

  ```cpp
  QMap<int, Task> m_allTasks;     // 唯一数据源
  QList<int>      m_pendingList;  // 按优先级排序的待分配队列
  QMap<int,int>   m_executingMap; // taskId -> robotId
  QList<int>      m_completedList, m_failedList, m_cancelledList;
  ```

- **排序**：`reorderPendingList()` 用 `std::stable_sort` 按优先级降序；
  **比较器内直接读 `m_allTasks`，不调用任何加锁成员函数**（这是死锁修复的核心）。
- **状态迁移 API**：`assignTask / finishTask / failTask / cancelTask / reassignTask`，
  每次迁移自动在 5 个索引列表间搬移，保证 `getPendingCount()` 等统计恒准确。

### 5.3 TaskScheduler（调度引擎）

调度算法对齐产品文档 6.1「最小负载优先 + 最短距离优先」：

```cpp
int TaskScheduler::selectBestRobotForTask(int taskId) {
    QList<int> c = filterCandidates(taskId);   // ① 过滤：仅空闲机器人
    sortByLoad(c);                              // ② LLF：任务数少者优先
    sortByDistance(c, task->getStartX(), ...);  // ③ SDF：离任务起点近者优先
    return c.first();
}
```

在三层基础上增强了工业场景必备的三条策略：

1. **优先级抢占**：当没有空闲机器人时，若新任务优先级**严格高于**某执行中任务，
   则释放该机器人、把被抢占任务 `reassignTask` 退回待分配队列。（无抢占时只记录一次等待日志。）
2. **超时回收**：执行中任务超过 `m_taskTimeoutMs = 180000` 未完成 → `failTask` + `finishRobotTask`，
   防止机器人因异常永远"卡在忙碌"。
3. **日志去重**：`m_lastWaitingTask` 记录上次提示的任务 ID，避免"无空闲机器人"每秒刷屏。

调度节拍由 `QTimer(1000ms)` 驱动 `doScheduling() = assignPendingTasks() + checkExecutingTasks()`；
`checkExecutingTasks` 用「机器人当前位置与任务终点距离 < 0.5」判定到达。

### 5.4 路径规划、避障与交通

**规划算法**：网格地图（1 格 = 1 世界单位），4 邻域 **BFS + 前驱回溯**求最短路径，
返回的是**格子中心的世界坐标序列**；对外接口按 A* 预留：

```cpp
QList<QPointF> planPathWorld(ax, ay, bx, by) const;                        // 普通
QList<QPointF> planPathWorldEx(ax, ay, bx, by, const QList<QPoint> &extra); // 带额外障碍格
bool           isReachable(ax, ay, bx, by) const;                           // 连通性预检
bool           isBlockedWorld(x, y) const;                                  // 越界/障碍判定
```

选择 BFS 的理由：4 邻域无权图下 BFS 即最短路，代码更短、无启发函数调试成本；
`extra` 参数把「其它机器人所在格」注入为临时障碍，与 A* 的 open/closed 逻辑完全兼容，
将来替换启发式搜索只需改函数体，不改调用方。

**四层避障策略**：

| 策略 | 位置 | 说明 |
| --- | --- | --- |
| 创建前预检 | `MainWindow` 的 `requestAddTask` 回调 | `isReachable` 为假直接弹窗拒绝，不产生"永远无法完成"的任务 |
| 规划期避障 | `planPathWorldEx` | 静态障碍 + 其它机器人格 |
| 行驶期让行 | `robotProximityBlocked` | 距任一机器人 < 0.8 单位则本拍不动（等待） |
| 阻塞重规划 | `stepBusyRobot` | 被停放机器人/充电桩挡住时，**限频 600ms** 把对方格作为额外障碍重算绕行路径 |

**充电桩格互斥**：`blockedByCharger(robotId, x, y)` 判断目标点是否落在某充电桩半径 0.5 内；
只有 `m_chargeTarget[robotId]` 正好是**该桩**的机器人可以进入，其它机器人一律拦下，
实现"机器人不能占用充电桩车位"。

**让道解卡 `resolveConflicts()`**：两机器人距离 < 0.9 时，让"非执行中/ID 较大"的一方沿垂直方向
偏移 1.0 格（依次尝试两个垂直方向，取第一个非障碍且不越界的落点）。
（对应的 UI 按钮因需求变更已移除，但 API 保留，供后续自动交通管制调用。）

### 5.5 运动与电量模拟（`stepRobots`，250ms/拍）

模拟模式（`m_simulateMovement == true`）下，每拍按状态分派：

- **Busy**：`stepBusyRobot` —— 首次为该任务拼接 `当前格→起点` + `起点→终点` 两段路径并缓存
  （`m_robotPlan / m_robotPlanIdx / m_robotPlanTask`），之后按 `speed = min(配置速度, 上限12)` 逐点推进；
  路径点间距小于 0.35 视为已到达并取下一个。
- **Charging**：`stepToward` 沿避障路径走向目标桩；到桩后按 `m_chargePerSec = 8 %/s` 回充，
  充满 或（≥80% 且存在待分配任务）→ 恢复 `Idle`。
- **Idle**：`goChargeIfLow` —— 电量 ≤ `m_lowChargeLevel = 30%` 时自动转入 `Charging`
  （`Charging` 状态不参与调度候选，所以充电期间不会被派活）；否则向最近充电桩巡游停靠。

**掉电模型**（`drainBattery`）：

```cpp
ratio = min(speed / maxSpeed, 1.0);
drop  = distance * m_drainPerUnit(0.22) * (0.5 + 0.5 * ratio);   // 速度越快，单位距离耗电越多
```

**出生点 `nextSpawnPos()`**：以第一个充电桩为圆心、半径 ≤4 的圆内随机取整点，避开障碍与越界，
保证新机器人不会生成在墙里；`MainWindow` 在新增机器人时用它预填对话框，
且**仅当用户在对话框里填了非 (0,0) 坐标时才覆盖**。

**默认充电桩 / 家**：`homePoint()` 取网格正中的格中心 `(int(cols*0.5)+0.5, int(rows*0.5)+0.5)`，
保证桩落在格子内部而不是压在网格线上。

### 5.6 通信层

见第 6 节协议详解。实现要点：

- 双协议共存：以 `0xAA 0x55` 帧头区分——是二进制帧就按帧解析，否则按 `\n` 切**JSON 行**。
- **粘包/半包**：每连接维护 `QByteArray m_buffer`，数据不足一帧/一行就 `break` 等下一包。
- **重同步**：帧尾不是 `0xDD 0xEE` 时只丢弃 2 字节帧头后重新扫描，而不是丢弃整段缓冲。
- **心跳**：1000ms 周期检查，超过 3000ms 未收到任何有效报文 → `robotHeartbeatTimeout` →
  `MainWindow` 把机器人置 `Offline`；期间周期性广播 `0x04` 心跳帧。
- **自动登记**：首次携带 `id` 的报文即视为注册；主窗口收到 `robotConnected` 时如果该 ID 不存在则自动建档，
  实现"机器人上线即被纳管"。

### 5.7 数据持久化（DataManager）

- 存档路径：`<运行目录>/Data/data.json`（大写 `Data` 目录，不存在自动 `mkdir`）。
- 结构：`{ "robots": [...], "tasks": [...], "config": {tcpPort, schedulerInterval, enableReturnHome, lowBatteryThreshold} }`。
- 机器人与任务序列化**保留全部业务字段**（含 `load/accel/maxLoad`、任务三段时间戳）。
- `RobotController::loadData` 采用**事务式替换**：先停调度 → 清空两个 Manager → 逐条重建 → 恢复 `enableReturnHome` → 恢复调度运行状态。
- 另有 `loadRobotsOnly()`：只替换机器人集合，不动任务，对应菜单「载入地图与机器人(不含任务)」。

### 5.8 账号与权限（UserManager + LoginWindow）

- 账号库 `Data/users.json`：`{ "users": [ {"username","password","role"} ] }`。
- **唯一管理员兜底**：`ensureDefaultAdmin()` 在每次 `load()` 后执行——`koni` 不存在则创建（`koni/123`），
  存在但角色被改坏则强制改回 `admin`。因此**数据集被清空/损坏也不会把自己锁在门外**。
- **注册的账号一律是普通用户**（`addUser` 的 `forceAdmin` 默认为 `false`），杜绝自助提权。
- `removeUser` 禁止删除 `koni`，并禁止删除最后一个管理员。
- 登录窗口选择**运行模式**：`模拟机器人(自动移动演示)` / `TCP 接入真实机器人`，
  以构造参数传入 `MainWindow(parent, simulate, isAdmin)`。

**权限矩阵**：

| 功能 | 管理员 | 普通用户 |
| --- | :-: | :-: |
| 地图设计（画障碍/擦除/尺寸/存载/充电桩/清空） | ✅ | ❌（画布强制切到"新建任务"工具） |
| 新增/编辑/删除机器人、清空全部机器人 | ✅ | ❌（按钮隐藏） |
| 新建任务（画布点选起终点） | ✅ | ✅ |
| 删除任务 / 回收 / 取消执行 / 强制停止机器人 | ✅ | ❌ |
| 查看列表、监控、日志 | ✅ | ✅ |
| 账号管理菜单 | ✅ | ❌ |

### 5.9 UI 层

**MainWindow**（`mainwindow.ui` + 代码混合）：
左侧 `QSplitter` 上半是画布宿主 `canvasHost`，右侧 `rightPanel` 自上而下为
机器人列表 → 任务列表 → 调度控制 → 机器人信息 `QTextBrowser`；底部日志用 `QTabWidget` 分「系统日志 / 任务日志」。

- **地图门控**：`mapChanged` 信号触发 `applyMapGate()`——未画出/载入地图前，
  机器人表、任务表、所有增删按钮、调度按钮统一 `setEnabled(false)`，从流程上避免"空网格直线穿障"和"无地图就调度"。
- **调度前强制推网格**：`开始调度` / `立即调度一次` 的槽里先 `setMapGrid(...)` 同步最新障碍，
  因为 `mapChanged` 是异步信号，存在时序空档。
- **信号汇聚**：控制器信号 → `appendLog` + `refreshRobotTable/refreshTaskTable/updateStatusBar`，
  UI 刷新只在这几个函数里发生，便于统一维护。
- **ID 分配**：`getNextRobotId()` 从 10001 起、`getNextTaskId()` 从 1 起，
  都带 `while (existing.contains(id)) ++id;`，即使用户手改过 ID 或删除过实体也不会重号。

**MapEditorWidget**（无 `.ui`，纯代码 `QPainter` 自绘）：

- **布局**：`paintEvent` 中 `left=8, top=m_topH(60)`，格子边长 `m_cell=16`（**按需求锁定，滚轮不再缩放**），
  默认 `50 列 × 30 行`，棋盘底色 + 浅灰网格线。
- **五种工具**（下拉框）：`0 画障碍 / 1 擦除 / 2 新建任务 / 3 设充电桩 / 4 查看选择`。
  擦除工具**同时清除障碍与该格的充电桩**（`removeChargerNear`），符合"橡皮擦所见即所除"的直觉。
- **按钮按场景收容**（`setDesignMode`）：切到「新建任务」时隐藏 保存/导入/清空/尺寸/充电桩/全部机器人 等设计类按钮，
  改为显示 `删除上一个任务`、`清空任务`；其余模式反过来。避免一行十几个按钮挤在一起。
- **画布式新建任务**：第一点画绿色虚线框并提示"请选择任务的终点"，第二点提交 `requestAddTask(起点, 终点, 优先级)`；
  起终点相同会被拒绝。优先级 `QComboBox` 提供 1~4（默认 2）。
- **叠加渲染 `drawOverlay`**：按任务 ID 从 6 色调色板取色绘制路径（`planPathWorld` 结果），
  绿/红圆点标起终点并标注任务号；紫色带 ⚡ 的圆画充电桩；
  机器人按状态着色（空闲绿 / 执行橙 / 故障红 / 离线灰 / 充电蓝 / 低电粉红），
  外圈画**一格大小的虚线碰撞体积框**，圆下方标 `R<id>` 与电量百分比。

---

## 6. 通信协议详解

### 6.1 二进制帧结构

```
┌────────┬──────────┬────────┬───────────────┬────────┬────────┐
│ 帧头 2B│ 长度 2B  │ 功能1B │  数据 N B     │ 校验1B │ 帧尾2B │
│ AA 55  │ 大端     │  func  │  载荷(N)      │  XOR   │ DD EE  │
└────────┴──────────┴────────┴───────────────┴────────┴────────┘
总长 = 8 + N；校验 = 长度高字节 ^ 长度低字节 ^ func ^ data[0..N-1]
```

| 功能码 | 方向 | 含义 | 实现状态 |
| :-: | --- | --- | :-: |
| `0x01` | 机器人 → RCS | 状态上报（JSON 载荷：x/y/battery/status） | ✅ |
| `0x02` | RCS → 机器人 | 任务下发（taskId/startX/startY/endX/endY/priority） | ✅ |
| `0x04` | RCS → 机器人 | 心跳（空载荷） | ✅ |
| `0x03` | RCS → 机器人 | 路径下发 | ⬜ 待实现 |
| `0x05` | 机器人 → RCS | 告警上报 | ⬜ 待实现 |
| `0x06` | RCS → 机器人 | 控制指令（暂停/恢复/急停） | ⬜ 待实现 |
| `0xF0/0xF1` | 双向 | ACK / NACK | ⬜ 待实现 |

### 6.2 JSON 行协议（兼容调试）

以 `\n` 结尾的纯文本行，便于用 `nc`/`telnet` 手工联调：

```jsonc
{"id":10001}                                        // 注册(首次含 id 即登记)
{"x":1.5,"y":3.0,"battery":88,"status":0}           // 状态上报(status 为 RobotStatus 整型)
```

### 6.3 联调方式

```bash
# 终端 A：以 TCP 模式启动上位机(登录后选择"TCP 接入真实机器人")，监听 8888
# 终端 B：启动两个模拟机器人
./tools/robotClient/build/robotClient 10001 127.0.0.1 8888 1000
./tools/robotClient/build/robotClient 10002 127.0.0.1 8888 1000
```

模拟器行为：连上后先发注册行，之后每周期发 `0x01` 帧上报位置/电量/状态；
断线后 `2s` 间隔重连，最多 5 次。

---

## 7. 构建、运行与调试

### 7.1 ⚠️ 必须使用影子构建（Shadow Build）

```bash
cd /path/to/AVGRobot
mkdir -p build/vscode && cd build/vscode
qmake ../../AVGRobot.pro
make -j4
./AVGRobot
```

**不要**在源码根目录直接 `make`。在源码目录构建会把 `ui_*.h`、`moc_*.cpp`、`Makefile` 生成到源码根，
而 `.pro` 的 `INCLUDEPATH += $$PWD` 会让编译器**优先命中源码根那份过期头文件**，
从而出现"新加的控件在 `ui->` 里找不到"这类幽灵问题（详见 8.1）。

### 7.2 VS Code

- `Ctrl+Shift+B` → **build** 任务：智能判断——若 `build/vscode/Makefile` 存在且 `AVGRobot.pro`
  不比它新，就只 `make`；否则先 `qmake` 再 `make`。改了 `.pro`（增删文件）无需手动 qmake。
- `F5` → 调试（`preLaunchTask: build`）；`Ctrl+F5` 可跑 `run` 任务（工作目录已设为 `build/vscode`）。
- `.vscode/c_cpp_properties.json` 已把 8 个模块目录全部加入 `includePath`，跨模块 `#include "robot.h"` 不再报红。

### 7.3 附带模拟器

```bash
cd tools/robotClient && mkdir -p build && cd build
qmake ../robotClient.pro && make -j4
```

### 7.4 运行期文件

| 文件 | 位置 | 内容 |
| --- | --- | --- |
| `Data/users.json` | 工作目录 | 账号库（首次运行自动生成 `koni/123`） |
| `Data/data.json` | 工作目录 | 机器人 + 任务 + 配置存档（菜单「保存数据」生成） |
| `Data/map.json` | 工作目录 | 网格地图（`cols` / `rows` / `obstacles` / `start` / `end`） |

---

## 8. 开发过程中的问题与解决 ⭐

这一节是本文档的重点：按"现象 → 根因 → 解决"记录全部已修复的缺陷与设计迭代。

### 8.1 编译/构建类问题

| # | 现象 | 根因 | 解决 |
| :-: | --- | --- | --- |
| 1 | `.ui` 里新增的 `textBrowser` 在 `.cpp` 里 `ui->textBrowser` 报"没有成员" | 源码根目录残留上一次**在源码目录构建**产生的旧 `ui_mainwindow.h`；`INCLUDEPATH += $$PWD` 使其优先级高于构建目录的新头文件，**旧头文件遮蔽了新头文件** | 删除源码根的 `ui_*.h` / `moc_*` / `Makefile` / `.qmake.stash`；此后**一律在 `build/vscode` 影子构建**，并在 `.gitignore` 中忽略这些生成的中间文件 |
| 2 | 同类问题在 `RobotDialog` 上复发（`editSpeed` 找不到） | 同上：又有一次在源码根执行了 `make` | 同上；已把"影子构建"写进第 7 节作为强制约定 |
| 3 | `class RobotController` 链接期 "multiple definition" | `Robot/` 与 `Task/` 下各有一份 `RobotController` 实现（早期分层遗留） | 删除 `Robot/robotcontroller.*`，保留 `Task/` 下作为唯一聚合门面 |
| 4 | `TaskManager` 编译报"析构函数未声明" | 头文件声明了 `~TaskManager()` 但实现里定义了，头里漏声明 | 补齐头文件声明 |
| 5 | **（本次）`robotcontroller.cpp:238` `error: 'g' 不是一个类型名`，整个工程编译失败** | 一次误编辑在 `clearFinishedTasks()` 与 `forceStopRobot()` 之间留下了一个游离字符 `g` | 删除该字符，`build/vscode` 重新 `make -j4` 通过，产物 `AVGRobot` 正常生成 |

### 8.2 运行时死锁 / 崩溃类问题（最严重）

| # | 现象 | 根因 | 解决 |
| :-: | --- | --- | --- |
| 6 | **创建第二个任务时界面卡死**（第一个任务正常） | `TaskManager::reorderPendingList()` 的排序比较器里调用了加锁的 `getTask()`，而调用方**已经持有 `m_mutex`**；`QMutex` 是**非递归锁**，同一线程二次加锁 → **自死锁**。第一个任务时比较器只被调用 0~1 次未触发，第二个任务比较次数变多必然触发 | 比较器改为**直接读 `m_allTasks`**，不再经过任何加锁成员函数。同时排查并修复了同类写法：`printTasksByStatus()`、`RobotManager::printAllRobots()` |
| 7 | 遍历机器人时偶发崩溃 | 同上类问题：锁内发射业务信号 → 槽函数回调加锁接口 | 确立编码约定：**锁内只允许发射纯日志信号；所有业务信号必须在 `QMutexLocker` 作用域结束、即解锁之后发射**（`RobotManager` 头注释已写明此约定） |
| 8 | `resolveConflicts` 单元测试段错误 | 测试用例传入非法 IP `"a"`，`Robot::isValidIp` 校验失败导致 `addRobot` 返回 `false`，随后代码对不存在的机器人解引用 | 测试改用合法 IP（如 `192.168.1.1`）；同时把让道位移从 0.5 格提高到 **1.0 格**——因为机器人停止间距是 0.8 格，0.5 格位移后距离仅 0.40 仍在阈值内，卡位没真正解除；改后距离 1.08 > 0.9，测试通过 |

### 8.3 业务逻辑一致性类问题

| # | 现象 | 根因 | 解决 |
| :-: | --- | --- | --- |
| 9 | 任务创建后一直"待分配"，调度器视而不见 | `Task` 的 7 参数构造函数**没有初始化 `m_status`**，其值为随机内存，`getPendingTaskIds()` 过滤不到 | 构造函数显式初始化 `Pending / assignedRobotId=-1 / 三个时间为空` |
| 10 | 删除任务后机器人**永远保持"忙碌"**，再也不能接单 | 任务删除只改了 `TaskManager`，没有清理 `RobotManager` 里对应的 `curTaskId` 与 `status`——即"机器人忙碌但任务已不存在"的双向失同步 | 在 `RobotController` 里把**所有会移除任务的路径统一为"先释放机器人，再动任务"**：`removeTask / clearAllTasks / removeNewestTask / clearFinishedTasks / removeAllRobots` 全部覆盖 |
| 11 | 点「清空任务」把正在执行的任务也清掉，机器人集体卡在忙碌 | 语义混淆了"清空全部"与"清理已完结" | 拆成两个 API：`clearFinishedTasks()`（仅清 Completed/Failed/Cancelled，UI 的「清空任务」按钮用它）与 `clearAllTasks()`（内部先释放所有执行中机器人，保证不产生僵尸忙碌态） |
| 12 | 任务无法回收/取消，机器人被长期占用 | 缺少"执行中任务退回队列"的入口 | 新增 `recycleTask()`（任意态 → 回 pending 并释放机器人）与 `cancelExecutingTask()`；UI 提供「回收任务」「取消执行」两个按钮 |
| 13 | 机器人异常（如离线）导致任务**永远执行不完** | 没有执行时长上限 | 调度器增加 `m_taskTimeoutMs = 180000` 超时回收：置失败 + 释放机器人 |
| 14 | 高优先级任务排在队尾干等 | 只有 FIFO + 优先级排序，没有资源抢占 | 调度器增加抢占逻辑：无空闲机器人时，若新任务优先级严格高于某执行中任务，则抢占其机器人，被抢占任务退回队列 |
| 15 | 日志被"没有空闲机器人"刷屏 | 每秒一拍无脑输出 | 用 `m_lastWaitingTask` 记录上次提示的任务 ID，只在任务首次开始等待时提示一次 |
| 16 | 程序重启后机器人/任务全丢 | 没有持久化 | 新增 `DataManager`（JSON）+ 菜单「保存数据 / 载入数据」，载入采用事务式替换并恢复调度运行状态 |
| 17 | TCP 模式下调度分配了任务，**真机收不到指令** | 只做了上行解析，缺下行通道 | `MainWindow` 订阅 `RobotController::taskAssigned`，组装 JSON 后用 `sendToRobot` 以 `0x02` 帧下发；实测日志出现"已向机器人 X 下发任务 Y" |
| 18 | 机器人心跳断了还显示"在线" | 没有超时判定 | `TcpRobotServer` 增加 1000ms 周期心跳检查，3000ms 无报文 → `robotHeartbeatTimeout` → 置 `Offline`；且"曾判离线后有报文"会自动恢复在线 |

### 8.4 通信协议类问题

| # | 现象 | 根因 | 解决 |
| :-: | --- | --- | --- |
| 19 | 机器人上报的二进制帧**永远解析不出数据**（JSON 行协议正常） | XOR 校验的循环上界写成了 `i <= 2 + dlen`，把**校验字节与帧尾**也纳入了异或，导致校验恒不通过 | 修正为 `i <= 4 + dlen`（覆盖 长度2B + 功能码1B + 数据 dlen）。这是最隐蔽的一类 bug——长度计算差 2，现象却是"数据静默丢失" |
| 20 | 线路噪声导致的伪帧头会让解析器**永久错位** | 遇到非法帧尾时直接丢弃整段缓冲 | 改为只丢弃 2 字节帧头后重新扫描同步，健壮性显著提升 |
| 21 | 机器人上报缺少位置字段时，位置被错误地更新为 (0,0) | 无法区分"上报了 0"与"没上报" | 引入 `hasPos` 标记：只有报文中同时含 `x` 和 `y` 才更新位置；电量/状态同样用 `>=0` 哨兵值判定 |
| 22 | 曾用 `std::nanf` 做"无效值"哨兵导致编译失败 | 目标工具链不支持该符号 | 改为 `0.0 + hasPos` 布尔标记，零依赖且语义清晰 |

### 8.5 路径规划 / 避障 / 交通类问题

| # | 现象 | 根因 | 解决 |
| :-: | --- | --- | --- |
| 23 | 机器人**直线穿过障碍物** | 移动模拟只做"朝目标直走"，没有用网格信息 | 引入 `planPathWorld`（BFS 最短路），`stepBusyRobot/stepToward` 全部改为沿路径点行进 |
| 24 | 刚启动就点调度，机器人仍然穿墙 | `mapChanged` 是异步信号，调度启动瞬间控制器里的网格还是空的 | 「开始调度」「立即调度一次」按钮的槽内**先强制 `setMapGrid(...)`** 再启动 |
| 25 | 任务目标在障碍里或被墙隔开，机器人**原地抽搐永不完成** | 创建任务时没有可达性校验 | `isReachable()` BFS 连通性预检；不可达则弹窗拒绝创建并写错误日志 |
| 26 | 机器人互相堵死（对头/追尾） | 没有机器人间避让 | 三层处理：① `robotProximityBlocked`（<0.8 单位即停）；② 被挡时**限频 600ms** 把其它机器人所在格作为额外障碍重规划绕行；③ `resolveConflicts` 让道解卡 |
| 27 | 重规划过频导致路径抖动、日志刷屏 | 每拍都重规划 | `m_lastReplan` 做 600ms 节流 |
| 28 | 停放的机器人堵住通道后，执行中的机器人**完全卡死** | 静态障碍里没有"其它机器人" | `planPathWorldEx` 增加 `extra` 额外障碍参数，把停放机器人格注入为临时障碍重算 |
| 29 | 只要路径规划一改，"起点→终点"两段路径衔接处会来回抖动一格 | 两段路径在接点处重复追加了同一个格子中心 | 拼接时判断 `plan.last() != pt` 去重，并跳过第二段的第一个重复点 |
| 30 | 首个任务使用空网格导致直线穿障（与 #24 同类但触发路径不同） | 同上 | 同上；两处入口都做了强制同步 |

### 8.6 电量 / 充电桩类问题

| # | 现象 | 根因 | 解决 |
| :-: | --- | --- | --- |
| 31 | 电量只随任务完成跳变，与行驶距离无关 | 没有掉电模型 | `drainBattery(robotId, distance, speed)` 按**距离 × 速度系数**掉电（速度越快单位距离耗电越多） |
| 32 | 机器人低速/超速：速度配置无上限，仿真里"瞬移" | 未做速度钳制 | 引入 `m_maxRobotSpeed = 12`，模拟与实际移动均取 `min(配置速度, 上限)` |
| 33 | 低电机器人仍然被派活，跑到半路"饿死" | 没有自动充电策略 | 电量 ≤ 30% 且空闲 → 状态转 `Charging`（**该状态不进入调度候选**）→ 沿避障路径回最近充电桩 → 按 8%/s 回充；充满或（≥80% 且有任务等待）恢复 `Idle` |
| 34 | 充电桩画在网格线上，视觉与占位都不对 | 直接用格子左上角坐标作为桩位置 | 新增 `homePoint()/cellWorldCenter()`，统一取**格中心** `(x+0.5, y+0.5)`；默认桩放在地图正中格 |
| 35 | 机器人停在充电桩车位上，别人的车充不了电 | 没有车位互斥 | `blockedByCharger()`：目标点落在某桩半径 0.5 内时，只有"正在前往该桩"的机器人可进入，其余全部拦下 |
| 36 | 橡皮擦只擦障碍，擦不掉充电桩 | 擦除工具语义不完整 | 擦除时同时调用 `removeChargerNear(x, y)` 清除该格充电桩 |

### 8.7 交互 / 易用性类问题（来自使用反馈的迭代）

| # | 反馈 / 现象 | 解决 |
| :-: | --- | --- |
| 37 | "新建机器人 ID 默认从 10001 开始递增" | `m_nextRobotId = 10001` 且 `getNextRobotId()` 跳过已占用 ID；任务 ID 同理由 1 起递增 |
| 38 | "提交任务只能手填坐标，太麻烦" | 把任务创建搬到画布：工具切「新建任务」后点两点即起终点，并在日志提示"请选择任务终点" |
| 39 | "地图设计和监控分成两个界面，来回切很烦" | 合并为**单一画布**：地图编辑、机器人/任务实时叠加、画布式新建任务同一视图完成；删除旧的 `view/mapwidget.*` |
| 40 | "按钮太多，一行放不下" | 按场景收容（`setDesignMode`）：设计模式显示地图类按钮，新建任务模式显示任务管理按钮；「开始调度/行动」按钮挪到画布工具栏 |
| 41 | "清空任务 / 删除上一个任务应该放在新建任务菜单下" | 两个按钮移入画布第二行，且**仅在工具=新建任务时可见** |
| 42 | "默认充电桩要放在格子内，且机器人不能进别人的充电桩格" | 见 #34、#35 |
| 43 | "又卡住了"（机器人对头僵局） | 见 #26、#28 |
| 44 | "需要一个强制停止机器人的按钮" | 新增 `forceStopRobot(id)`：任务退回待分配 + 机器人置空闲；画布「查看/选择」点选机器人后点该按钮生效（仅管理员） |
| 45 | "'解除卡位'按钮点了没反应，没用" | 移除该按钮；卡位改由**自动重规划**（#26/#28）解决，`resolveConflicts()` 作为 API 保留备用 |
| 46 | "缩放把格子缩得看不清，锁定 16px/格" | `m_cell = 16` 固定，`wheelEvent` 改为 `accept()` 空实现（滚轮不再缩放） |
| 47 | "滚轮缩放没生效"（实现期） | 根因是函数名写成 `mouseWheelEvent`，**没有真正 override** `QWidget::wheelEvent`；改名后生效（随后按 #46 锁定） |
| 48 | "右侧希望能直接看到机器人/任务列表和单独的信息区" | 重写 `mainwindow.ui`：右侧列表面板 + 调度控制 + `robotInfo` 详情区；表格选中行或画布点选机器人都刷新详情 |
| 49 | "没有地图就乱点调度" | 地图门控（见 5.9）：未画地图前右侧面板全部禁用 |
| 50 | "想让机器人'忙碌'时自身 taskId 与 TaskManager 同步" | 见 #10；`assignTaskToRobot` / `finishRobotTask` 保证双向一致，并在所有删除路径补齐释放 |
| 51 | "想有登录和管理员/普通用户区分" | 新增 `UserManager` + `LoginWindow`；JSON 持久化账号；唯一管理员 `koni/123`；普通用户按钮级隐藏 |

---

## 9. 与产品文档的差距 / 后续计划

对照 `02-AVG物流机器人任务调度系统.docx` 的功能框图，**已完成**与**待完善**如下：

**已完成（可演示）**：任务管理（创建/分配/取消/回收/队列）、机器人管理（接入/状态同步/参数配置/上下线）、
调度引擎（LLF+SDF、抢占、超时回收）、路径规划（网格最短路 + 动态避障 + 重规划 + 路径可视化）、
TCP 通信（服务端、帧解析、心跳、任务下发）、监控告警（状态监控、低电/离线告警、日志）、
数据持久化、账号与权限、地图编辑与可视化。

**待完善**：

| 项 | 现状 | 建议 |
| --- | --- | --- |
| 路径算法 | BFS 最短路（4 邻域无权图下等价最优） | 替换为 A* 启发式搜索以支持大网格；接口 `planPathWorldEx` 已预留，仅需替换函数体 |
| 通信方式 | 仅 TCP | 文档要求 TCP/UDP/串口(`QSerialPort`)/CAN 全兼容，可用策略模式抽象 `ITransport` |
| 通信并发 | 单线程事件循环 | 文档要求"每连接独立线程"，可引入 `QThread`/线程池或将解析下沉 |
| 协议完整性 | 0x01/0x02/0x04 已实现 | 补 0x03 路径下发、0x05 告警上报、0x06 控制指令、0xF0/0xF1 ACK/NACK 与超时重传 |
| 交通管制 | 就近停 + 重规划 + 让道 | 文档的"区域锁、路口仲裁、死锁检测"未实现 |
| 数据统计 | 无 | 任务完成率/效率分析图表（QCustomPlot） |
| 上层对接 | 无 | WMS/MES 接口（REST/MQ） |
| 多地图/多用户隔离 | 单地图、账号不隔离数据 | 按账号或项目维度组织存档目录 |
| 安全性 | 密码明文存储 | 加盐哈希（QCryptographicHash / bcrypt） |
| 并发模型 | 全部在主线程 | 大量机器人时把调度/规划移出 UI 线程 |
| 小清理 | `Data/Data.json` 是 0 字节的历史遗留文件（实际存档为小写 `data.json`） | 删除以免混淆（Linux 下大小写敏感） |
| 充电桩未持久化 | `map.json` 只存障碍与起终点，充电桩仅存在内存（重启回到默认桩） | 在 `saveToFile/loadFromFile` 中把 `chargers` 一并序列化 |
| 编译告警 | 若干未使用参数、`QWheelEvent::pos()` 弃用告警 | 用 `Q_UNUSED` 与 `position().toPoint()` 清理 |

---

## 10. 快速上手

1. **构建**（务必影子构建）
   ```bash
   cd AVGRobot && mkdir -p build/vscode && cd build/vscode
   qmake ../../AVGRobot.pro && make -j4
   ```
2. **运行** `./AVGRobot`
3. **登录**：管理员 `koni / 123`（唯一管理员；自己注册的账号均为普通用户）
4. **选模式**：先选「模拟机器人(自动移动演示)」体验完整闭环
5. **画地图**：工具「画障碍」拖拽画几堵墙，留出通道
6. **加机器人**：右上「添加机器人」→ ID 自动预填 10001，出生点已自动预填在充电桩附近 → 确定
7. **建任务**：工具切「新建任务」，选优先级（默认 2）→ 画布点起点 → 点终点
8. **开始行动**：点画布上「开始调度/行动」→ 观察机器人沿彩色路径（避开障碍）驶向终点，
   电量随行驶下降，终点到达后任务变"已完成"
9. **验证异常处理**：把任务终点画在被墙完全隔开的区域 → 会弹窗"不可达，任务创建失败"
10. **TCP 联调**：重新登录选「TCP 接入真实机器人」，另开终端跑
    `./tools/robotClient/build/robotClient 10001` 观察上报与任务下发

---

## 11. 编码约定（维护须知）

1. **绝不在源码根目录 `make`**——只在 `build/vscode` 影子构建（见 8.1）。
2. **持有 `QMutex` 时不得调用任何加锁成员函数**；需要数据就在锁内直接读成员容器（见 8.2 #6）。
3. **业务信号在解锁后发射**，锁内只发日志信号（见 8.2 #7）。
4. **跨模块的一致性操作写在 `RobotController` 里**，UI 不直接操作 Manager（如"删任务必释放机器人"）。
5. **新增/删除源文件后必须重跑 `qmake`**（VS Code 的 build 任务已自动判断）。
6. UI 刷新集中在 `refreshRobotTable / refreshTaskTable / updateStatusBar / updateRobotInfo` 四处。

---

*文档版本：v1.0 ｜ 对应代码：`build/vscode` 全量构建通过（`make -j4` exit 0）*
