#include "robotdialog.h"
#include "ui_robotdialog.h"
#include <QPushButton>
#include <QMessageBox>

RobotDialog::RobotDialog(bool editMode, int robotId, QWidget *parent)
    : QDialog(parent), ui(new Ui::RobotDialog), m_editMode(editMode), m_editRobotId(robotId)
{
    ui->setupUi(this);

    setupUI();

    connect(ui->btnOk, &QPushButton::clicked, this, &RobotDialog::onBtnOkClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &RobotDialog::onBtnCancelClicked);
}

RobotDialog::~RobotDialog()
{
    delete ui;
}

void RobotDialog::setupUI()
{
    if (m_editMode)
    {
        setWindowTitle("编辑机器人");
        ui->editRobotId->setEnabled(false);
    }
    else
    {
        setWindowTitle("添加机器人");
        ui->editRobotId->setEnabled(true);

        ui->editRobotId->setText(QString::number(m_editRobotId));
        ui->editIp->setText("127.0.0.1");
        ui->editX->setText("0.0");
        ui->editY->setText("0.0");
        ui->editBattery->setText("100");
        ui->editSpeed->setText("5.0");
        ui->editAccel->setText("1.5");
        ui->editMaxLoad->setText("500");
    }

    adjustSize();
}

void RobotDialog::setEditData(int id, const QString &ip, float x, float y, int battery, float speed, float accel, int maxLoad)
{
    ui->editRobotId->setText(QString::number(id));
    ui->editIp->setText(ip);
    ui->editX->setText(QString::number(x, 'f', 1));
    ui->editY->setText(QString::number(y, 'f', 1));
    ui->editBattery->setText(QString::number(battery));
    ui->editSpeed->setText(QString::number(speed, 'f', 1));
    ui->editAccel->setText(QString::number(accel, 'f', 1));
    ui->editMaxLoad->setText(QString::number(maxLoad));
}

int RobotDialog::getRobotId() const
{
    return ui->editRobotId->text().toInt();
}

QString RobotDialog::getIp() const
{
    return ui->editIp->text().trimmed();
}

float RobotDialog::getX() const
{
    return ui->editX->text().toFloat();
}

float RobotDialog::getY() const
{
    return ui->editY->text().toFloat();
}

int RobotDialog::getBattery() const
{
    return ui->editBattery->text().toInt();
}

float RobotDialog::getSpeed() const
{
    return ui->editSpeed->text().toFloat();
}

float RobotDialog::getAccel() const
{
    return ui->editAccel->text().toFloat();
}

int RobotDialog::getMaxLoad() const
{
    return ui->editMaxLoad->text().toInt();
}

void RobotDialog::onBtnOkClicked()
{
    // 简单验证
    if (ui->editRobotId->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入机器人ID");
        return;
    }

    bool ok;
    int id = ui->editRobotId->text().toInt(&ok);
    if (!ok || id < 0)
    {
        QMessageBox::warning(this, "提示", "请输入有效的机器人ID");
        return;
    }

    if (ui->editIp->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, "提示", "请输入IP地址");
        return;
    }

    int battery = ui->editBattery->text().toInt(&ok);
    if (!ok || battery < 0 || battery > 100)
    {
        QMessageBox::warning(this, "提示", "请输入有效的电量 (0-100)");
        return;
    }

    float speed = ui->editSpeed->text().toFloat(&ok);
    if (!ok || speed < 0)
    {
        QMessageBox::warning(this, "提示", "请输入有效的速度 (≥0)");
        return;
    }

    accept();
}

void RobotDialog::onBtnCancelClicked()
{
    reject();
}