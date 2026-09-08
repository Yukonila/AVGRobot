#include "mapwidget.h"
#include "Task/robotcontroller.h"
#include "mapeditor.h"

#include <QPainter>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QLabel>
#include <cmath>

RobotMapWidget::RobotMapWidget(QWidget *parent)
    : QWidget(parent), m_chkReturnHome(nullptr), m_btnHome(nullptr), m_controller(nullptr), m_mapSrc(nullptr), m_enableReturnHome(false)
{
    setMinimumSize(420, 360);

    // ---- 创建控件 ----
    m_btnHome = new QPushButton("全部回原点", this);
    m_chkReturnHome = new QCheckBox("启用机器人回原点", this);
    m_chkReturnHome->setChecked(false);

    // 全部回原点：只让空闲机器人回原点(不打断执行中的)
    connect(m_btnHome, &QPushButton::clicked, this, [this]()
            {
        if (!m_controller) return;
        m_controller->returnIdleRobotsToHome();
        update(); });

    connect(m_chkReturnHome, &QCheckBox::toggled,
            this, [this](bool checked)
            {
                m_enableReturnHome = checked;
                if (m_controller)
                {
                    m_controller->setEnableReturnHome(checked);
                }
                emit returnHomeToggled(checked);
                update(); // 重绘地图
            });

    // 布局：左边"全部回原点"按钮，右边复选框
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(m_btnHome);
    topLayout->addStretch(1);
    topLayout->addWidget(m_chkReturnHome);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(topLayout);
    mainLayout->addStretch(1);

    setLayout(mainLayout);
}

QSize RobotMapWidget::minimumSizeHint() const
{
    return QSize(420, 360);
}

void RobotMapWidget::setController(RobotController *controller)
{
    m_controller = controller;
    if (!m_controller)
        return;

    // 同步复选框状态
    m_enableReturnHome = m_controller->isReturnHomeEnabled();
    m_chkReturnHome->setChecked(m_enableReturnHome);

    // 数据变化时重绘
    connect(m_controller, &RobotController::robotAdded, this, [this](int)
            { update(); });
    connect(m_controller, &RobotController::robotRemoved, this, [this](int)
            { update(); });
    connect(m_controller, &RobotController::robotStatusChanged, this, [this](int, RobotStatus, RobotStatus)
            { update(); });
    connect(m_controller, &RobotController::robotPositionChanged, this, [this](int, float, float)
            { update(); });
    connect(m_controller, &RobotController::taskAdded, this, [this](int)
            { update(); });
    connect(m_controller, &RobotController::taskRemoved, this, [this](int)
            { update(); });
    connect(m_controller, &RobotController::taskAssigned, this, [this](int, int)
            { update(); });
    connect(m_controller, &RobotController::taskFinished, this, [this](int)
            { update(); });
    connect(m_controller, &RobotController::taskCancelled, this, [this](int)
            { update(); });
    connect(m_controller, &RobotController::robotReturnedHome, this, [this](int)
            { update(); });

    update();
}

void RobotMapWidget::setMapSource(const MapEditorWidget *map)
{
    m_mapSrc = map;
    if (m_mapSrc)
    {
        connect(m_mapSrc, &MapEditorWidget::mapChanged, this, [this]() { update(); });
        update();
    }
}

void RobotMapWidget::setEnableReturnHome(bool enable)
{
    m_enableReturnHome = enable;
    m_chkReturnHome->setChecked(enable);
    if (m_controller)
    {
        m_controller->setEnableReturnHome(enable);
    }
    update();
}

bool RobotMapWidget::isReturnHomeEnabled() const
{
    return m_enableReturnHome;
}

QColor RobotMapWidget::colorForStatus(int status) const
{
    switch (static_cast<RobotStatus>(status))
    {
    case RobotStatus::Idle:
        return QColor("#2e9e44");
    case RobotStatus::Busy:
        return QColor("#e08c00");
    case RobotStatus::Error:
        return QColor("#d12f2f");
    case RobotStatus::Offline:
        return QColor("#9aa0a6");
    case RobotStatus::Charging:
        return QColor("#2f6fd1");
    case RobotStatus::Lowbattery:
        return QColor("#c2185b");
    default:
        return QColor("#3a3a3a");
    }
}

void RobotMapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int W = width();
    const int H = height();
    const int margin = 36;

    // 地图绘制区域（避开右上角复选框）
    const int topMargin = 40;

    // ---- 收集所有需要显示的点 ----
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool haveAny = false;

    auto absorb = [&](float x, float y)
    {
        if (!haveAny)
        {
            minX = maxX = x;
            minY = maxY = y;
            haveAny = true;
            return;
        }
        if (x < minX)
            minX = x;
        if (x > maxX)
            maxX = x;
        if (y < minY)
            minY = y;
        if (y > maxY)
            maxY = y;
    };

    if (m_controller)
    {
        for (int id : m_controller->getAllRobotIds())
        {
            const Robot *r = m_controller->getRobot(id);
            if (r)
                absorb(r->getPx(), r->getPy());
        }
        for (int id : m_controller->getAllTaskIds())
        {
            const Task *t = m_controller->getTask(id);
            if (t)
            {
                absorb(t->getStartX(), t->getStartY());
                absorb(t->getEndX(), t->getEndY());
            }
        }
    }
    absorb(0, 0);

    float spanX = (maxX - minX);
    float spanY = (maxY - minY);
    minX -= spanX * 0.08f;
    maxX += spanX * 0.08f;
    minY -= spanY * 0.08f;
    maxY += spanY * 0.08f;
    if (spanX < 1e-6f)
    {
        minX -= 5;
        maxX += 5;
    }
    if (spanY < 1e-6f)
    {
        minY -= 5;
        maxY += 5;
    }

    // 世界坐标 -> 屏幕坐标
    auto toScreen = [&](float wx, float wy, float &sx, float &sy)
    {
        sx = margin + (wx - minX) / (maxX - minX) * (W - 2 * margin);
        sy = topMargin + (1.0f - (wy - minY) / (maxY - minY)) * (H - margin - topMargin);
    };

    // ---- 网格 ----
    p.setPen(QPen(QColor(230, 230, 230), 1));
    int gridN = 10;
    for (int i = 0; i <= gridN; ++i)
    {
        float wx = minX + (maxX - minX) * i / gridN;
        float x0, y0, x1, y1;
        toScreen(wx, minY, x0, y0);
        toScreen(wx, maxY, x1, y1);
        p.drawLine(QPointF(x0, y0), QPointF(x1, y1));
    }
    for (int i = 0; i <= gridN; ++i)
    {
        float wy = minY + (maxY - minY) * i / gridN;
        float x0, y0, x1, y1;
        toScreen(minX, wy, x0, y0);
        toScreen(maxX, wy, x1, y1);
        p.drawLine(QPointF(x0, y0), QPointF(x1, y1));
    }

    // ---- 原点 ----
    float ox, oy;
    toScreen(0, 0, ox, oy);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(120, 120, 120), 1, Qt::DotLine));
    p.drawEllipse(QPointF(ox, oy), 10, 10);
    p.drawText(QPointF(ox + 12, oy - 12), "原点(0,0)");

    if (!m_controller)
        return;

    // ---- 叠加地图编辑器里的障碍(以"1个格子=1个世界单位"绘制) ----
    if (m_mapSrc)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#8ea0ad"));
        for (int r = 0; r < m_mapSrc->rows(); ++r)
        {
            for (int c = 0; c < m_mapSrc->columns(); ++c)
            {
                if (!m_mapSrc->isObstacle(c, r))
                    continue;
                // 格 (c,r) 覆盖世界矩形 [c,c+1] x [r,r+1]
                float x1, y1, x2, y2;
                toScreen((float)c, (float)r, x1, y1);
                toScreen((float)(c + 1), (float)(r + 1), x2, y2);
                p.drawRect(QRectF(QPointF(x1, y1), QPointF(x2, y2)));
            }
        }
    }

    // ---- 任务：起点->终点 ----
    for (int id : m_controller->getAllTaskIds())
    {
        const Task *t = m_controller->getTask(id);
        if (!t)
            continue;

        float sx, sy, ex, ey;
        toScreen(t->getStartX(), t->getStartY(), sx, sy);
        toScreen(t->getEndX(), t->getEndY(), ex, ey);

        QColor dim(150, 150, 150);
        p.setPen(QPen(dim, 1, Qt::DashLine));
        p.drawLine(QPointF(sx, sy), QPointF(ex, ey));

        // 终点(红方块)
        p.setPen(QPen(QColor("#b71c1c"), 2));
        p.setBrush(QColor("#ef5350"));
        p.drawRect(QRectF(ex - 5, ey - 5, 10, 10));
        p.setPen(QColor("#7f0000"));
        p.drawText(QPointF(ex + 7, ey - 7), QString::number(id));
    }

    // ---- 机器人 ----
    for (int id : m_controller->getAllRobotIds())
    {
        const Robot *r = m_controller->getRobot(id);
        if (!r)
            continue;

        float sx, sy;
        toScreen(r->getPx(), r->getPy(), sx, sy);

        QColor c = colorForStatus(static_cast<int>(r->getStatus()));
        p.setPen(QPen(QColor("white"), 2));
        p.setBrush(c);
        p.drawEllipse(QPointF(sx, sy), 8, 8);

        p.setPen(c.darker(150));
        p.setFont(QFont("Sans", 8, QFont::Bold));
        p.drawText(QPointF(sx + 11, sy + 3), QString("R%1").arg(id));

        // 如果机器人是忙碌状态，画一条指向任务终点的虚线
        if (r->getStatus() == RobotStatus::Busy)
        {
            int taskId = r->getTask();
            if (taskId >= 0)
            {
                const Task *t = m_controller->getTask(taskId);
                if (t)
                {
                    float ex, ey;
                    toScreen(t->getEndX(), t->getEndY(), ex, ey);
                    p.setPen(QPen(QColor(200, 150, 50), 1, Qt::DotLine));
                    p.drawLine(QPointF(sx, sy), QPointF(ex, ey));
                }
            }
        }
    }

    // ---- 图例 ----
    int ly = topMargin + 4;
    struct
    {
        const char *name;
        QColor col;
    } legend[] = {
        {"空闲", colorForStatus(static_cast<int>(RobotStatus::Idle))},
        {"忙碌/执行", colorForStatus(static_cast<int>(RobotStatus::Busy))},
        {"故障", colorForStatus(static_cast<int>(RobotStatus::Error))},
        {"离线", colorForStatus(static_cast<int>(RobotStatus::Offline))},
    };
    p.setFont(QFont("Sans", 8));
    for (auto &lg : legend)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(lg.col);
        p.drawEllipse(QPointF(margin + 4, ly - 3), 4, 4);
        p.setPen(QColor("#444444"));
        p.drawText(QPointF(margin + 12, ly), lg.name);
        ly += 14;
    }
}

void RobotMapWidget::computeWorldToScreen(float &minX, float &maxX, float &minY, float &maxY,
                                          float &outScaleX, float &outOffsetX,
                                          float &outScaleY, float &outOffsetY) const
{
    minX = 0;
    minY = 0;
    maxX = 0;
    maxY = 0;
    bool haveAny = false;
    auto absorb = [&](float x, float y)
    {
        if (!haveAny)
        {
            minX = maxX = x;
            minY = maxY = y;
            haveAny = true;
            return;
        }
        minX = qMin(minX, x);
        maxX = qMax(maxX, x);
        minY = qMin(minY, y);
        maxY = qMax(maxY, y);
    };
    if (m_controller)
    {
        for (int id : m_controller->getAllRobotIds())
        {
            const Robot *r = m_controller->getRobot(id);
            if (r)
                absorb(r->getPx(), r->getPy());
        }
        for (int id : m_controller->getAllTaskIds())
        {
            const Task *t = m_controller->getTask(id);
            if (t)
            {
                absorb(t->getStartX(), t->getStartY());
                absorb(t->getEndX(), t->getEndY());
            }
        }
    }
    absorb(0, 0);
    float spanX = maxX - minX;
    float spanY = maxY - minY;
    minX -= spanX * 0.08f;
    maxX += spanX * 0.08f;
    minY -= spanY * 0.08f;
    maxY += spanY * 0.08f;
    if (spanX < 1e-6f)
    {
        minX -= 5;
        maxX += 5;
    }
    if (spanY < 1e-6f)
    {
        minY -= 5;
        maxY += 5;
    }

    const int W = width();
    const int H = height();
    const int margin = 36;
    const int topMargin = 40;

    outOffsetX = margin;
    outScaleX = (W - 2 * margin) / (maxX - minX);
    outScaleY = (H - margin - topMargin) / (maxY - minY);
    outOffsetY = topMargin; // y 轴需翻转：sy = offsetY + scaleY*(maxY - wy)
    // 为了让机器人位置 y 越低显示越靠上，这里用 maxY 作参考（见 robotAt）
    // 简单起见：记录 topMargin 在 outOffsetY，y 用翻转公式
}

bool RobotMapWidget::robotAt(const QPointF &screen, int &outRobotId) const
{
    if (!m_controller)
        return false;
    float minX, maxX, minY, maxY, scaleX, offX, scaleY, offY;
    computeWorldToScreen(minX, maxX, minY, maxY, scaleX, offX, scaleY, offY);
    const int topMargin = (int)offY;

    const float sx = screen.x();
    const float sy = screen.y();
    int bestId = -1;
    double best = 1e18;
    for (int id : m_controller->getAllRobotIds())
    {
        const Robot *r = m_controller->getRobot(id);
        if (!r)
            continue;
        float px = offX + (r->getPx() - minX) * scaleX;
        float py = topMargin + (maxY - r->getPy()) * scaleY;
        double d = std::sqrt((px - sx) * (px - sx) + (py - sy) * (py - sy));
        if (d < best)
        {
            best = d;
            bestId = id;
        }
    }
    if (bestId < 0 || best > 16.0)
        return false;
    outRobotId = bestId;
    return true;
}

void RobotMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        int robotId = -1;
        if (robotAt(event->pos(), robotId))
        {
            emit robotSelected(robotId);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}
