#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidgetItem>
#include "Task/robotcontroller.h"
#include "mapwidget.h"
#include "mapeditor.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, bool simulate = true);
    ~MainWindow();

private slots:
    // 按钮点击槽
    void onBtnAddRobotClicked();
    void onBtnEditRobotClicked();
    void onBtnDeleteRobotClicked();
    void onBtnRefreshClicked();

    // 任务按钮槽
    void onBtnAddTaskClicked();
    void onBtnDeleteTaskClicked();

    // 机器人列表双击
    void onTableRobotDoubleClicked(int row, int column);

    // Controller 信号
    void onLogMessage(const QString &msg, int level);
    void onRobotAdded(int robotId);
    void onRobotRemoved(int robotId);
    void onRobotStatusChanged(int robotId, RobotStatus oldStatus, RobotStatus newStatus);
    void onRobotPositionChanged(int robotId, float x, float y);
    void onRobotBatteryChanged(int robotId, int battery);

signals:
    // 请求退出登录并返回登录界面（由登录窗口监听处理）
    void logoutRequested();
    

private:
    // 辅助方法
    void appendLog(const QString &msg, int level = 0);
    QString statusToString(RobotStatus status) const;
    QString taskStatusToString(TaskStatus status) const;
    void refreshRobotTable();
    void refreshTaskTable();
    void updateStatusBar();
    void updateSchedulerState();
    int getSelectedRobotId() const;
    int getSelectedTaskId() const;
    int getNextRobotId();
    int getNextTaskId();
    void createMenuBar();

    Ui::MainWindow *ui;
    RobotController *m_controller;
    RobotMapWidget *m_map;
    MapEditorWidget *m_mapEditor;
    int m_nextRobotId = 10001; // 新建机器人ID从10001开始自增
    int m_nextTaskId = 1;      // 新建任务ID从1开始自增
};

#endif // MAINWINDOW_H