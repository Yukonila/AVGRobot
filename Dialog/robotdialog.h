#ifndef ROBOTDIALOG_H
#define ROBOTDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class RobotDialog;
}
QT_END_NAMESPACE

class RobotDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RobotDialog(bool editMode = false, int robotId = -1, QWidget *parent = nullptr);
    ~RobotDialog();

    void setEditData(int id, const QString &ip, float x, float y, int battery);

    int getRobotId() const;
    QString getIp() const;
    float getX() const;
    float getY() const;
    int getBattery() const;

private slots:
    void onBtnOkClicked();
    void onBtnCancelClicked();

private:
    void setupUI();

    Ui::RobotDialog *ui;
    bool m_editMode;
    int m_editRobotId;
};

#endif // ROBOTDIALOG_H
