#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QPoint>

class RobotController;
class MapEditorWidget;

class RobotMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RobotMapWidget(QWidget *parent = nullptr);
    ~RobotMapWidget() = default;

    void setController(RobotController *controller);
    // 绑定地图编辑数据源，把障碍叠加显示到本视图
    void setMapSource(const MapEditorWidget *map);
    void setEnableReturnHome(bool enable);
    bool isReturnHomeEnabled() const;

    QSize minimumSizeHint() const override;

signals:
    void returnHomeToggled(bool enabled);
    // 点击某个机器人 → 通知外界查看/记录该机器人信息
    void robotSelected(int robotId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QColor colorForStatus(int status) const;
    // 命中测试：把鼠标屏幕坐标换算成世界坐标找最近的机器人
    bool robotAt(const QPointF &screen, int &outRobotId) const;
    void computeWorldToScreen(float &minX, float &maxX, float &minY, float &maxY,
                              float &outScaleX, float &outOffsetX,
                              float &outScaleY, float &outOffsetY) const;

    QCheckBox *m_chkReturnHome;
    QPushButton *m_btnHome;
    RobotController *m_controller;
    const MapEditorWidget *m_mapSrc;
    bool m_enableReturnHome;
};

#endif // MAPWIDGET_H
