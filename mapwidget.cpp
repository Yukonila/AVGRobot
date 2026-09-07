#include "mapwidget.h"
#include "Task/robotcontroller.h"

#include <QPainter>
#include <QFontMetrics>
#include <cmath>

RobotMapWidget::RobotMapWidget(QWidget *parent)
    : QWidget(parent), m_controller(nullptr)
{
    setMinimumSize(360, 300);
}

QSize RobotMapWidget::minimumSizeHint() const
{
    return QSize(360, 300);
}

void RobotMapWidget::setController(RobotController *controller)
{
    m_controller = controller;
    if (!m_controller)
        return;

    // 数据变化时重绘
    connect(m_controller, &RobotController::robotAdded, this, [this](int) { update(); });
    connect(m_controller, &RobotController::robotRemoved, this, [this](int) { update(); });
    connect(m_controller, &RobotController::robotStatusChanged, this, [this](int, RobotStatus, RobotStatus) { update(); });
    connect(m_controller, &RobotController::robotPositionChanged, this, [this](int, float, float) { update(); });
    connect(m_controller, &RobotController::taskAdded, this, [this](int) { update(); });
    connect(m_controller, &RobotController::taskRemoved, this, [this](int) { update(); });
    connect(m_controller, &RobotController::taskAssigned, this, [this](int, int) { update(); });
    connect(m_controller, &RobotController::taskFinished, this, [this](int) { update(); });
    connect(m_controller, &RobotController::taskCancelled, this, [this](int) { update(); });
    update();
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

    // ---- 收集所有需要显示的点，计算世界坐标范围(自适应缩放) ----
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
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
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
    absorb(0, 0); // 保证原点在视野内

    // 加一点边距，避免贴边
    float spanX = (maxX - minX);
    float spanY = (maxY - minY);
    minX -= spanX * 0.08f;
    maxX += spanX * 0.08f;
    minY -= spanY * 0.08f;
    maxY += spanY * 0.08f;
    if (spanX < 1e-6f) { minX -= 5; maxX += 5; }
    if (spanY < 1e-6f) { minY -= 5; maxY += 5; }

    // 世界坐标 -> 屏幕坐标 (y 轴向上翻转)
    auto toScreen = [&](float wx, float wy, float &sx, float &sy)
    {
        sx = margin + (wx - minX) / (maxX - minX) * (W - 2 * margin);
        sy = margin + (1.0f - (wy - minY) / (maxY - minY)) * (H - 2 * margin);
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
        // 路径线
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
    }

    // ---- 图例 ----
    int ly = margin + 4;
    struct { const char *name; QColor col; } legend[] = {
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
