#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "robotdialog.h"
#include <QDateTime>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QTextCursor>
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>
#include <QPushButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_controller(new RobotController(this))
{
    ui->setupUi(this);

    // 初始化
    createMenuBar();
    refreshRobotTable();
    refreshTaskTable();
    updateStatusBar();
    updateSchedulerState();

    // ========== 连接 Controller 信号 ==========
    connect(m_controller, &RobotController::logMessage,
            this, &MainWindow::onLogMessage);
    connect(m_controller, &RobotController::robotAdded,
            this, &MainWindow::onRobotAdded);
    connect(m_controller, &RobotController::robotRemoved,
            this, &MainWindow::onRobotRemoved);
    connect(m_controller, &RobotController::robotStatusChanged,
            this, &MainWindow::onRobotStatusChanged);
    connect(m_controller, &RobotController::robotPositionChanged,
            this, &MainWindow::onRobotPositionChanged);
    connect(m_controller, &RobotController::robotBatteryChanged,
            this, &MainWindow::onRobotBatteryChanged);

    // ========== 任务 / 调度信号 → 刷新任务表与日志 ==========
    connect(m_controller, &RobotController::taskAdded,
            this, [this](int taskId) { appendLog(QString("[任务] %1 已创建").arg(taskId), 0); refreshTaskTable(); });
    connect(m_controller, &RobotController::taskRemoved,
            this, [this](int taskId) { appendLog(QString("[任务] %1 已删除").arg(taskId), 1); refreshTaskTable(); });
    connect(m_controller, &RobotController::taskAssigned,
            this, [this](int taskId, int robotId) { appendLog(QString("[调度] 任务 %1 → 机器人 %2").arg(taskId).arg(robotId), 0); refreshTaskTable(); refreshRobotTable(); updateStatusBar(); });
    connect(m_controller, &RobotController::taskFinished,
            this, [this](int taskId) { appendLog(QString("[任务] %1 已完成").arg(taskId), 0); refreshTaskTable(); refreshRobotTable(); updateStatusBar(); });
    connect(m_controller, &RobotController::taskFailed,
            this, [this](int taskId) { appendLog(QString("[任务] %1 异常").arg(taskId), 2); refreshTaskTable(); });
    connect(m_controller, &RobotController::taskCancelled,
            this, [this](int taskId) { appendLog(QString("[任务] %1 已取消").arg(taskId), 1); refreshTaskTable(); });
    connect(m_controller, &RobotController::schedulerStarted,
            this, [this]() { updateSchedulerState(); });
    connect(m_controller, &RobotController::schedulerStopped,
            this, [this]() { updateSchedulerState(); });

    // ========== 连接机器人按钮信号 ==========
    connect(ui->btnAddRobot, &QPushButton::clicked,
            this, &MainWindow::onBtnAddRobotClicked);
    connect(ui->btnEditRobot, &QPushButton::clicked,
            this, &MainWindow::onBtnEditRobotClicked);
    connect(ui->btnDeleteRobot, &QPushButton::clicked,
            this, &MainWindow::onBtnDeleteRobotClicked);
    connect(ui->btnRefresh, &QPushButton::clicked,
            this, &MainWindow::onBtnRefreshClicked);

    // 双击机器人行 → 编辑
    connect(ui->tableRobot, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onTableRobotDoubleClicked);

    // ========== 连接任务按钮信号 ==========
    connect(ui->btnAddTask, &QPushButton::clicked,
            this, [this]() { onBtnAddTaskClicked(); });
    connect(ui->btnDeleteTask, &QPushButton::clicked,
            this, [this]() { onBtnDeleteTaskClicked(); });
    connect(ui->btnRefreshTask, &QPushButton::clicked,
            this, [this]() { refreshTaskTable(); appendLog("任务列表已刷新", 1); });

    // ========== 调度控制按钮 ==========
    connect(ui->btnStartScheduler, &QPushButton::clicked,
            this, [this]() { m_controller->startScheduler(1000); });
    connect(ui->btnStopScheduler, &QPushButton::clicked,
            this, [this]() { m_controller->stopScheduler(); });
    connect(ui->btnScheduleOnce, &QPushButton::clicked,
            this, [this]() { m_controller->scheduleOnce(); refreshTaskTable(); });

    // ========== 地图监控页 ==========
    m_map = new RobotMapWidget(ui->tabMain);
    m_map->setController(m_controller);
    ui->tabMain->addTab(m_map, "地图监控");

    appendLog("系统初始化完成", 0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ========== 菜单栏 ==========

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("文件(&F)");

    // 保存数据
    QAction *saveAction = fileMenu->addAction("保存数据(&S)");
    connect(saveAction, &QAction::triggered, this, [this]()
            {
        if (m_controller->saveData())
            appendLog("数据已保存", 0); });

    // 载入数据
    QAction *loadAction = fileMenu->addAction("载入数据(&I)...");
    connect(loadAction, &QAction::triggered, this, [this]()
            {
        if (QMessageBox::question(this, "载入数据",
                                  "载入会覆盖当前机器人和任务数据，是否继续？",
                                  QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        if (m_controller->loadData())
        {
            refreshRobotTable();
            refreshTaskTable();
            updateStatusBar();
            updateSchedulerState();
        }
        else
        {
            QMessageBox::warning(this, "载入失败", "未找到或无法解析数据文件");
        } });
    fileMenu->addSeparator();

    QAction *logoutAction = fileMenu->addAction("退出登录(&L)");
    connect(logoutAction, &QAction::triggered, this, [this]()
            {
        emit logoutRequested();
        this->close(); });
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("退出(&E)");
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu("帮助(&H)");
    QAction *aboutAction = helpMenu->addAction("关于(&A)");
    connect(aboutAction, &QAction::triggered, this, [this]()
            { QMessageBox::about(this, "关于本系统",
                                 "AVG 物流机器人任务调度系统 (RCS)\n\n"
                                 "版本: 1.0.0\n"
                                 "开发框架: Qt 5.12\n"
                                 "© 2026 All Rights Reserved"); });
}

// ========== 按钮槽函数 ==========

void MainWindow::onBtnAddRobotClicked()
{
    // 打开添加对话框（默认ID从10001开始自增）
    RobotDialog dialog(false, getNextRobotId(), this);
    if (dialog.exec() == QDialog::Accepted)
    {
        int id = dialog.getRobotId();
        QString ip = dialog.getIp();
        float x = dialog.getX();
        float y = dialog.getY();
        int battery = dialog.getBattery();
        float speed = dialog.getSpeed();

        if (m_controller->addRobot(id, ip))
        {
            // 添加成功后设置初始位置、电量和速度
            m_controller->updatePosition(id, x, y);
            m_controller->updateBattery(id, battery);
            m_controller->updateSpeed(id, speed);
            appendLog(QString("机器人 %1 添加成功").arg(id), 0);
            refreshRobotTable();
            updateStatusBar();
        }
        else
        {
            QMessageBox::warning(this, "添加失败", "机器人ID已存在或无效");
        }
    }
}

void MainWindow::onBtnEditRobotClicked()
{
    int robotId = getSelectedRobotId();
    if (robotId < 0)
    {
        QMessageBox::information(this, "提示", "请先选择一个机器人");
        return;
    }

    // 获取当前机器人数据
    const Robot *robot = m_controller->getRobot(robotId);
    if (!robot)
    {
        QMessageBox::warning(this, "错误", "机器人不存在");
        return;
    }

    // 打开编辑对话框
    RobotDialog dialog(true, robotId, this);
    dialog.setEditData(robotId, robot->getIp(), robot->getPx(), robot->getPy(),
                       robot->getBattery(), robot->getSpeed());

    if (dialog.exec() == QDialog::Accepted)
    {
        // 更新数据
        m_controller->updatePosition(robotId, dialog.getX(), dialog.getY());
        m_controller->updateBattery(robotId, dialog.getBattery());
        m_controller->updateSpeed(robotId, dialog.getSpeed());
        m_controller->updateRobotIp(robotId, dialog.getIp());

        appendLog(QString("机器人 %1 信息已更新").arg(robotId), 1);
        refreshRobotTable();
        updateStatusBar();
    }
}

void MainWindow::onBtnDeleteRobotClicked()
{
    int robotId = getSelectedRobotId();
    if (robotId < 0)
    {
        QMessageBox::information(this, "提示", "请先选择一个机器人");
        return;
    }

    // 确认删除
    if (QMessageBox::question(this, "确认删除",
                              QString("确定要删除机器人 %1 吗？").arg(robotId),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    if (m_controller->removeRobot(robotId))
    {
        appendLog(QString("机器人 %1 已删除").arg(robotId), 0);
        refreshRobotTable();
        updateStatusBar();
    }
    else
    {
        QMessageBox::warning(this, "删除失败", "机器人可能正在执行任务");
    }
}

void MainWindow::onBtnRefreshClicked()
{
    refreshRobotTable();
    updateStatusBar();
    appendLog("列表已刷新", 1);
}

void MainWindow::onBtnAddTaskClicked()
{
    // ===== 用代码构造"新建任务"对话框，避免额外新增文件 =====
    QDialog dlg(this);
    dlg.setWindowTitle("新建任务");
    dlg.setMinimumWidth(320);

    auto *idSpin = new QSpinBox(&dlg);
    idSpin->setRange(0, 999999);
    idSpin->setValue(1);

    auto *prioCombo = new QComboBox(&dlg);
    prioCombo->addItem("低", 0);
    prioCombo->addItem("中", 1);
    prioCombo->addItem("高", 2);

    auto *startX = new QDoubleSpinBox(&dlg);
    auto *startY = new QDoubleSpinBox(&dlg);
    auto *endX = new QDoubleSpinBox(&dlg);
    auto *endY = new QDoubleSpinBox(&dlg);
    for (QDoubleSpinBox *sb : {startX, startY, endX, endY})
    {
        sb->setRange(-10000, 10000);
        sb->setDecimals(1);
        sb->setSingleStep(0.5);
    }

    auto *descEdit = new QLineEdit(&dlg);

    auto *form = new QFormLayout;
    form->addRow("任务 ID:", idSpin);
    form->addRow("优先级:", prioCombo);
    form->addRow("起点 X:", startX);
    form->addRow("起点 Y:", startY);
    form->addRow("终点 X:", endX);
    form->addRow("终点 Y:", endY);
    form->addRow("描述:", descEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *vbox = new QVBoxLayout(&dlg);
    vbox->addLayout(form);
    vbox->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    int taskId = idSpin->value();
    int priority = prioCombo->currentData().toInt();

    if (!m_controller->addTask(taskId, priority,
                               (float)startX->value(), (float)startY->value(),
                               (float)endX->value(), (float)endY->value(),
                               descEdit->text().trimmed()))
    {
        QMessageBox::warning(this, "添加失败", "任务ID已存在或无效");
        return;
    }
    appendLog(QString("任务 %1 已创建").arg(taskId), 0);
    refreshTaskTable();
}

void MainWindow::onBtnDeleteTaskClicked()
{
    int taskId = getSelectedTaskId();
    if (taskId < 0)
    {
        QMessageBox::information(this, "提示", "请先选择一个任务");
        return;
    }

    if (QMessageBox::question(this, "确认删除",
                              QString("确定要删除任务 %1 吗？").arg(taskId),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    if (m_controller->removeTask(taskId))
    {
        appendLog(QString("任务 %1 已删除").arg(taskId), 0);
        refreshTaskTable();
    }
    else
    {
        QMessageBox::warning(this, "删除失败", "任务不存在");
    }
}

void MainWindow::onTableRobotDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    onBtnEditRobotClicked();
}

// ========== Controller 信号槽 ==========

void MainWindow::onLogMessage(const QString &msg, int level)
{
    appendLog(msg, level);
}

void MainWindow::onRobotAdded(int robotId)
{
    appendLog(QString("[信号] 机器人 %1 上线").arg(robotId), 0);
    refreshRobotTable();
    updateStatusBar();
}

void MainWindow::onRobotRemoved(int robotId)
{
    appendLog(QString("[信号] 机器人 %1 下线").arg(robotId), 1);
    refreshRobotTable();
    updateStatusBar();
}

void MainWindow::onRobotStatusChanged(int robotId, RobotStatus oldStatus, RobotStatus newStatus)
{
    appendLog(QString("[信号] 机器人 %1 状态: %2 → %3")
                  .arg(robotId)
                  .arg(statusToString(oldStatus))
                  .arg(statusToString(newStatus)),
              1);
    refreshRobotTable();
    updateStatusBar();
}

void MainWindow::onRobotPositionChanged(int robotId, float x, float y)
{
    refreshRobotTable();
}

void MainWindow::onRobotBatteryChanged(int robotId, int battery)
{
    refreshRobotTable();
}

// ========== 辅助方法 ==========

void MainWindow::refreshRobotTable()
{
    QTableWidget *table = ui->tableRobot;
    table->clearContents();

    QList<int> ids = m_controller->getAllRobotIds();
    table->setColumnCount(8);
    table->setRowCount(ids.size());

    // 设置列标题
    QStringList headers = {"ID", "IP", "位置X", "位置Y", "电量(%)", "速度", "状态", "任务ID"};
    table->setHorizontalHeaderLabels(headers);

    for (int i = 0; i < ids.size(); ++i)
    {
        int id = ids[i];
        const Robot *robot = m_controller->getRobot(id);
        if (!robot)
            continue;

        table->setItem(i, 0, new QTableWidgetItem(QString::number(id)));
        table->setItem(i, 1, new QTableWidgetItem(robot->getIp()));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(robot->getPx(), 'f', 1)));
        table->setItem(i, 3, new QTableWidgetItem(QString::number(robot->getPy(), 'f', 1)));
        table->setItem(i, 4, new QTableWidgetItem(QString::number(robot->getBattery()) + "%"));
        table->setItem(i, 5, new QTableWidgetItem(QString::number(robot->getSpeed(), 'f', 1)));
        table->setItem(i, 6, new QTableWidgetItem(statusToString(robot->getStatus())));
        table->setItem(i, 7, new QTableWidgetItem(QString::number(robot->getTask())));
    }
    // 调整列宽
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MainWindow::updateStatusBar()
{
    int total = m_controller->getRobotCount();
    int idle = m_controller->getIdleCount();
    int busy = m_controller->getBusyCount();
    int fault = m_controller->getFaultCount();

    statusBar()->showMessage(QString("共 %1 个机器人 | 空闲: %2 | 忙碌: %3 | 故障: %4")
                                 .arg(total)
                                 .arg(idle)
                                 .arg(busy)
                                 .arg(fault));
}

int MainWindow::getSelectedRobotId() const
{
    int row = ui->tableRobot->currentRow();
    if (row < 0)
    {
        return -1;
    }
    QTableWidgetItem *item = ui->tableRobot->item(row, 0);
    if (!item)
    {
        return -1;
    }
    return item->text().toInt();
}

int MainWindow::getNextRobotId()
{
    // 从 m_nextRobotId 开始找第一个未被占用的ID；若用户手动改过ID或删过，
    // 仍保证不冲突（跳过已存在的ID）
    int id = m_nextRobotId;
    QList<int> existing = m_controller->getAllRobotIds();
    while (existing.contains(id))
        ++id;

    m_nextRobotId = id + 1;
    return id;
}

void MainWindow::refreshTaskTable()
{
    QTableWidget *table = ui->tableTask;
    table->clearContents();

    QList<int> ids = m_controller->getAllTaskIds();
    table->setColumnCount(8);
    table->setRowCount(ids.size());

    QStringList headers = {"ID", "优先级", "起点X", "起点Y", "终点X", "终点Y", "状态", "分配机器人"};
    table->setHorizontalHeaderLabels(headers);

    for (int i = 0; i < ids.size(); ++i)
    {
        const Task *task = m_controller->getTask(ids[i]);
        if (!task)
            continue;

        const QString prio = task->getPriority() == 2 ? "高"
                            : task->getPriority() == 1 ? "中"
                                                       : "低";
        const QString robot = task->getAssignedRobotId() < 0
                                  ? "未分配"
                                  : QString::number(task->getAssignedRobotId());

        table->setItem(i, 0, new QTableWidgetItem(QString::number(task->getTaskId())));
        table->setItem(i, 1, new QTableWidgetItem(prio));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(task->getStartX(), 'f', 1)));
        table->setItem(i, 3, new QTableWidgetItem(QString::number(task->getStartY(), 'f', 1)));
        table->setItem(i, 4, new QTableWidgetItem(QString::number(task->getEndX(), 'f', 1)));
        table->setItem(i, 5, new QTableWidgetItem(QString::number(task->getEndY(), 'f', 1)));
        table->setItem(i, 6, new QTableWidgetItem(taskStatusToString(task->getStatus())));
        table->setItem(i, 7, new QTableWidgetItem(robot));
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void MainWindow::updateSchedulerState()
{
    bool running = m_controller->isSchedulerRunning();
    ui->schedulerStateLabel->setText(running ? "调度器: 运行中" : "调度器: 停止");
    ui->btnStartScheduler->setEnabled(!running);
    ui->btnStopScheduler->setEnabled(running);
}

int MainWindow::getSelectedTaskId() const
{
    int row = ui->tableTask->currentRow();
    if (row < 0)
        return -1;
    QTableWidgetItem *item = ui->tableTask->item(row, 0);
    if (!item)
        return -1;
    return item->text().toInt();
}

QString MainWindow::taskStatusToString(TaskStatus status) const
{
    switch (status)
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

void MainWindow::appendLog(const QString &msg, int level)
{
    QString prefix;
    switch (level)
    {
    case 0:
        prefix = "[INFO] ";
        break;
    case 1:
        prefix = "[DEBUG]";
        break;
    case 2:
        prefix = "[ERROR]";
        break;
    case 3:
        prefix = "[WARN] ";
        break;
    default:
        prefix = "[LOG]  ";
        break;
    }

    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->textLog->append(QString("%1 %2 %3").arg(timeStr, prefix, msg));

    QTextCursor cursor = ui->textLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textLog->setTextCursor(cursor);
}

QString MainWindow::statusToString(RobotStatus status) const
{
    switch (status)
    {
    case RobotStatus::None:
        return "None";
    case RobotStatus::Idle:
        return "空闲";
    case RobotStatus::Busy:
        return "忙碌";
    case RobotStatus::Error:
        return "故障";
    case RobotStatus::Offline:
        return "离线";
    case RobotStatus::Lowbattery:
        return "低电量";
    case RobotStatus::Charging:
        return "充电中";
    default:
        return "未知";
    }
}