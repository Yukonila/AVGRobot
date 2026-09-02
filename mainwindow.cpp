#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_robotmanager = new RobotManager;

    testfun();
    connect(m_robotmanager, &RobotManager::logMessage,
            this, &MainWindow::appendLog);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::appendLog(const QString &msg, int level)
{
    QString prefix;
    QString color;
    switch (level)
    {
    case 0:
        prefix = "[INFO]";
        color = "black";
        break;
    case 1:
        prefix = "[DEBUG]";
        color = "gray";
        break;
    case 2:
        prefix = "[ERROR]";
        color = "red";
        break;
    case 3:
        prefix = "[WARN]";
        color = "orange";
        break;
    default:
        prefix = "[LOG]";
        color = "black";
        break;
    }

    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logLine = QString("%1 %2 %3").arg(timeStr, prefix, msg);

    ui->textBrowser->append(logLine);
    QTextCursor cursor = ui->textBrowser->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textBrowser->setTextCursor(cursor);
}

QString MainWindow::statusToString(RobotStatus status)
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

void MainWindow::testfun()
{
    m_robotmanager->addRobot(1001, "192.181.100.58");
    
}
