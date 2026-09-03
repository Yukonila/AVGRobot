#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QTextCursor>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_robotmanager = new RobotManager;

    connect(m_robotmanager, &RobotManager::logMessage,
            this, &MainWindow::appendLog);

}

MainWindow::~MainWindow()
{
    m_robotmanager->clearAll();
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
    QTextBrowser *tb = ui->textBrowser;
    tb->clear();

    int pass = 0;
    int fail = 0;

    // report: 把结果同时写到 textBrowser 和 console(qDebug)，便于自动化核对
    auto report = [&](bool ok, const QString &what)
    {
        QString line = QString(ok ? "[PASS] " : "[FAIL] ") + what;
        tb->append(line);
        qDebug().noquote() << line;
        ok ? pass++ : fail++;
    };

    tb->append("========== Robot / RobotManager 自动化测试 ==========");

    // 测试期间断开 manager 自身日志，保证报告干净；结束时恢复
    QObject::disconnect(m_robotmanager, &RobotManager::logMessage,
                        this, &MainWindow::appendLog);

    // ============ 1. Robot 类基础方法 ============
    {
        Robot r;
        report(r.getId() == 0 && r.getBattery() == 100 && r.getStatus() == RobotStatus::Idle && r.getTask() == -1 && r.getSpeed() == 0.0f,
               "Robot 默认值正确(id=0,电量100,空闲,任务-1,速度0)");

        r.setAll(7, 1.5f, 2.5f, 80, 3.0f, 5, RobotStatus::Busy, "192.168.1.7");
        report(r.getId() == 7 && r.getBattery() == 80 && r.getTask() == 5 && qAbs(r.getPx() - 1.5f) < 1e-4f && qAbs(r.getPy() - 2.5f) < 1e-4f && r.getIp() == "192.168.1.7",
               "setAll + getters 往返正确");

        report(!r.isAvailable(), "Busy 状态下 isAvailable()==false");
        r.setStatus(RobotStatus::Idle);
        report(r.isAvailable(), "Idle 状态下 isAvailable()==true");

        r.setBattery(150); // 非法：>100
        report(r.getBattery() == 80, "setBattery(150) 越界被拒绝(值不变)");
        r.setBattery(60);
        report(r.getBattery() == 60, "setBattery(60) 合法");
        r.increaseBattery(30); // 60+30=90
        report(r.getBattery() == 90, "increaseBattery(30) 90");
        r.increaseBattery(50); // 90+50>100
        report(r.getBattery() == 90, "increaseBattery 超上限被拒绝");
        r.decreaseBattery(30); // 90-30=60
        report(r.getBattery() == 60, "decreaseBattery(30) 60");
        r.decreaseBattery(100); // 越界
        report(r.getBattery() == 60, "decreaseBattery 越界被拒绝");

        float speedBefore = r.getSpeed(); // setAll 后为 3.0
        r.setSpeed(-5);                   // 非法
        report(qAbs(r.getSpeed() - speedBefore) < 1e-4f && !r.getErrorMsg().isEmpty(),
               "setSpeed(负数)被拒绝并记录 errorMsg");
        r.setSpeed(2.0f);
        report(qAbs(r.getSpeed() - 2.0f) < 1e-4f, "setSpeed(2.0) 合法");

        r.setIp("999.999.1.1"); // 非法
        report(r.getIp() != "999.999.1.1", "setIp(非法)被拒绝");
        r.setIp("10.0.0.1");
        report(r.getIp() == "10.0.0.1", "setIp(合法)成功");

        report(Robot::isValidIp("192.168.0.1") && !Robot::isValidIp("abc") && !Robot::isValidIp(""),
               "isValidIp 静态方法");
        report(!r.printRobot().isEmpty(), "printRobot 输出非空");
    }

    // ============ 2. add / get / has / remove ============
    m_robotmanager->clearAll(); // 从干净状态开始

    report(m_robotmanager->addRobot(1, "192.168.1.1"), "addRobot(id,ip) 成功");
    report(m_robotmanager->addRobot(2, "192.168.1.2"), "addRobot 成功2");
    report(m_robotmanager->addRobot(3, "192.168.1.3"), "addRobot 成功3");

    report(m_robotmanager->hasRobot(1) && m_robotmanager->getRobotCount() == 3,
           "hasRobot + getRobotCount == 3");
    report(m_robotmanager->getRobot(1) != nullptr && m_robotmanager->getRobot(1)->getIp() == "192.168.1.1",
           "getRobot(存在) 返回对象且数据正确");
    report(m_robotmanager->getRobot(999) == nullptr, "getRobot(不存在) 返回空");
    report(!m_robotmanager->hasRobot(99), "hasRobot(不存在) == false");

    report(!m_robotmanager->addRobot(1, "192.168.1.9"), "重复 addRobot 被拒绝");
    report(!m_robotmanager->addRobot(-1, "1.2.3.4"), "addRobot(负id) 被拒绝");
    report(!m_robotmanager->addRobot(9, "bad_ip"), "addRobot(非法ip) 被拒绝");

    Robot rob;
    rob.setAll(4, 0.0f, 0.0f, 88, 0.0f, -1, RobotStatus::Idle, "10.0.0.4");
    report(m_robotmanager->addRobot(rob), "addRobot(Robot对象) 成功");
    report(m_robotmanager->getRobotCount() == 4, "addRobot 后总数==4");

    // ============ 3. 位置/电量/速度/状态 更新 ============
    report(m_robotmanager->updateRobotPosition(1, 12.5f, 33.0f), "updateRobotPosition");
    report(qAbs(m_robotmanager->getRobot(1)->getPx() - 12.5f) < 1e-4f && qAbs(m_robotmanager->getRobot(1)->getPy() - 33.0f) < 1e-4f,
           "位置已更新");
    report(!m_robotmanager->updateRobotPosition(999, 1, 1), "updateRobotPosition(不存在) 失败");

    report(m_robotmanager->updateRobotBattery(1, 15), "updateRobotBattery(1->15)");
    report(m_robotmanager->getRobot(1)->getBattery() == 15, "battery==15");

    report(m_robotmanager->updateRobotSpeed(2, 4.5f), "updateRobotSpeed(2->4.5)");
    report(qAbs(m_robotmanager->getRobot(2)->getSpeed() - 4.5f) < 1e-4f, "speed==4.5");

    report(m_robotmanager->updateRobotStatus(3, RobotStatus::Error), "updateRobotStatus(3->故障)");
    report(m_robotmanager->getRobot(3)->getStatus() == RobotStatus::Error, "3号状态==故障");

    // 组合更新：不存在的 id 失败
    report(!m_robotmanager->updateRobotStatus(999, 1.0f, 1.0f, 50, 1.0f, RobotStatus::Idle),
           "组合update(不存在) 失败");

    // ============ 4. 任务分配 / 完成 / 取消 ============
    report(m_robotmanager->assignTaskToRobot(2, 500), "assignTaskToRobot(2,任务500) 成功");
    report(m_robotmanager->getRobotCurrentTask(2) == 500, "getRobotCurrentTask==500");
    report(m_robotmanager->isRobotBusy(2), "isRobotBusy(2)==true");
    report(m_robotmanager->getRobot(2)->getStatus() == RobotStatus::Busy,
           "分配后 2号状态==忙碌");
    report(!m_robotmanager->assignTaskToRobot(2, 600), "忙碌中再分配失败");
    report(!m_robotmanager->assignTaskToRobot(999, 1), "assignTask(不存在) 失败");

    report(m_robotmanager->finishRobotTask(2), "finishRobotTask(2) 成功");
    report(m_robotmanager->getRobotCurrentTask(2) == -1 && !m_robotmanager->isRobotBusy(2) && m_robotmanager->getRobot(2)->getStatus() == RobotStatus::Idle,
           "完成任务后回到空闲、任务-1");
    report(!m_robotmanager->finishRobotTask(2), "无任务可完成时 finish 失败");

    report(m_robotmanager->assignTaskToRobot(1, 700), "assignTaskToRobot(1,700) 成功");
    report(m_robotmanager->cancelRobotTask(1), "cancelRobotTask(1) 成功");
    report(m_robotmanager->getRobot(1)->getTask() == -1, "取消后任务==-1");
    report(!m_robotmanager->cancelRobotTask(1), "无任务可取消时 cancel 失败");

    // 有任务时 remove 应被拒绝
    report(m_robotmanager->assignTaskToRobot(4, 800), "assignTaskToRobot(4,800) 成功");
    report(!m_robotmanager->removeRobot(4), "remove 忙碌机器人被拒绝");
    report(m_robotmanager->finishRobotTask(4), "finishRobotTask(4) 成功");
    report(m_robotmanager->removeRobot(4), "remove(空闲) 成功");

    // ============ 5. 状态查询与统计 ============
    // 目前: 1=空闲(电15,低电量) 2=空闲 3=故障
    report(!m_robotmanager->getIdleRobots().isEmpty() && m_robotmanager->getIdleRobots().contains(1),
           "getIdleRobots 含1号");
    report(m_robotmanager->getFaultRobots().contains(3), "getFaultRobots 含3号(故障)");
    report(m_robotmanager->getOnlineRobots().contains(2) && m_robotmanager->getOnlineRobots().contains(3),
           "getOnlineRobots 含2、3号(故障也算在线)");
    report(m_robotmanager->getLowBatteryRobots().contains(1), "getLowBatteryRobots 含1号(电量15<20)");
    report(!m_robotmanager->getLowBatteryRobots().contains(2), "getLowBatteryRobots 不含2号");

    report(m_robotmanager->getIdleCount() >= 2, "getIdleCount>=2");
    report(m_robotmanager->getFaultCount() == 1, "getFaultCount==1");
    report(m_robotmanager->getOnlineCount() >= 3, "getOnlineCount>=3");

    report(m_robotmanager->getCountByStatus(RobotStatus::Error) == 1,
           "getCountByStatus(故障)==1");
    report(m_robotmanager->getRobotsByStatus(RobotStatus::Idle).contains(1),
           "getRobotsByStatus(空闲) 含1号");

    // 恢复干净状态验证 remove / clearAll
    report(m_robotmanager->removeRobot(1), "removeRobot(1) 成功");
    report(!m_robotmanager->hasRobot(1), "移除后 hasRobot(1)==false");
    report(m_robotmanager->removeRobot(999) == false, "removeRobot(不存在) 失败");

    m_robotmanager->clearAll();
    report(m_robotmanager->getRobotCount() == 0, "clearAll 后总数==0");

    // ============ 6. 打印调试方法(覆盖修复过的 printAllRobots 死锁路径) ============
    for (int i = 1; i <= 3; i++)
        m_robotmanager->addRobot(i, "192.168.0.1");
    report(!m_robotmanager->printAllRobots().isEmpty(), "printAllRobots 输出非空");
    report(!m_robotmanager->printRobotBrief(1).isEmpty() && m_robotmanager->printRobotBrief(2).contains("Robot #2"),
           "printRobotBrief 输出正确");

    // ============ 汇总 ============
    tb->append(QString("---------- 测试完成: 通过 %1 项, 失败 %2 项 ----------")
                   .arg(pass)
                   .arg(fail));

    // 恢复 manager 自身日志到窗口
    connect(m_robotmanager, &RobotManager::logMessage,
            this, &MainWindow::appendLog);

    // 补几个演示机器人，同时演示 addRobot 的"已添加"日志能正常显示
    m_robotmanager->clearAll();
    for (int i = 0; i < 10; i++)
        m_robotmanager->addRobot(1000 + i, "192.181.100.58");
    tb->append("---------- 当前机器人列表 ----------");
    tb->append(m_robotmanager->printAllRobots());
}
