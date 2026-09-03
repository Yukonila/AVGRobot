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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_controller(new RobotController(this))
{
    ui->setupUi(this);

    // 初始化
    createMenuBar();
    refreshRobotTable();
    updateStatusBar();

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

    // ========== 连接按钮信号 ==========
    connect(ui->btnAddRobot, &QPushButton::clicked,
            this, &MainWindow::onBtnAddRobotClicked);
    connect(ui->btnEditRobot, &QPushButton::clicked,
            this, &MainWindow::onBtnEditRobotClicked);
    connect(ui->btnDeleteRobot, &QPushButton::clicked,
            this, &MainWindow::onBtnDeleteRobotClicked);
    connect(ui->btnRefresh, &QPushButton::clicked,
            this, &MainWindow::onBtnRefreshClicked);

    // 双击表格行 → 编辑
    connect(ui->tableRobot, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onTableRobotDoubleClicked);

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
            { QMessageBox::about(this, "关于 AVG 调度系统",
                                 "AVG 物流机器人任务调度系统\n\n"
                                 "版本: 1.0.0\n"
                                 "开发框架: Qt 5.12\n"
                                 "© 2026 All Rights Reserved"); });
}

// ========== 按钮槽函数 ==========

void MainWindow::onBtnAddRobotClicked()
{
    // 打开添加对话框
    RobotDialog dialog(false, -1, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        int id = dialog.getRobotId();
        QString ip = dialog.getIp();
        float x = dialog.getX();
        float y = dialog.getY();
        int battery = dialog.getBattery();

        if (m_controller->addRobot(id, ip))
        {
            // 添加成功后设置初始位置和电量
            m_controller->updatePosition(id, x, y);
            m_controller->updateBattery(id, battery);
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
    dialog.setEditData(robotId, robot->getIp(), robot->getPx(), robot->getPy(), robot->getBattery());

    if (dialog.exec() == QDialog::Accepted)
    {
        // 更新数据
        m_controller->updatePosition(robotId, dialog.getX(), dialog.getY());
        m_controller->updateBattery(robotId, dialog.getBattery());
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
    table->setColumnCount(7);
    table->setRowCount(ids.size());

    // 设置列标题
    QStringList headers = {"ID", "IP", "位置X", "位置Y", "电量(%)", "状态", "任务ID"};
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
        table->setItem(i, 5, new QTableWidgetItem(statusToString(robot->getStatus())));
        table->setItem(i, 6, new QTableWidgetItem(QString::number(robot->getTask())));
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