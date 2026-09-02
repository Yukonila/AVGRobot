#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "Robot/robotManager.h"
#include "robotTcpManager.h"

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
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void testfun();
    void appendLog(const QString &msg, int level);
    QString statusToString(RobotStatus status);

private:
    Ui::MainWindow *ui;
    RobotManager *m_robotmanager;
};
#endif // MAINWINDOW_H
