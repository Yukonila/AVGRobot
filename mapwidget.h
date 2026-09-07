#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>

class RobotController;

// 简易平面地图可视化：
// 用自绘坐标网格展示机器人当前位置(按状态配色)与任务起终点/路径
class RobotMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RobotMapWidget(QWidget *parent = nullptr);

    void setController(RobotController *controller);

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor colorForStatus(int status) const;

    RobotController *m_controller;
};

#endif // MAPWIDGET_H
