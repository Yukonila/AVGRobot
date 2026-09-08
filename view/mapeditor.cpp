#include "mapeditor.h"
#include "Task/robotcontroller.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QSignalBlocker>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <cmath>

static QString robotStatusName(int status);
static QColor robotColor(int status);

QString MapEditorWidget::defaultMapPath()
{
    QDir dir = QDir::current();
    if (!dir.exists("Data"))
        dir.mkdir("Data");
    return dir.absolutePath() + "/Data/map.json";
}

MapEditorWidget::MapEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_cols(50)
    , m_rows(30)
    , m_cell(18)
    , m_grid(m_cols * m_rows, 0)
    , m_start(-1, -1)
    , m_end(-1, -1)
    , m_tool(0)
    , m_dragging(false)
    , m_controller(nullptr)
    , m_pickStart(-1, -1)
    , m_toolCombo(nullptr)
    , m_btnClear(nullptr)
    , m_btnSave(nullptr)
    , m_btnLoad(nullptr)
    , m_btnNewTask(nullptr)
    , m_colSpin(nullptr)
    , m_rowSpin(nullptr)
    , m_btnApplySize(nullptr)
    , m_chkAutoHome(nullptr)
    , m_btnAllHome(nullptr)
    , m_status(nullptr)
    , m_topH(30)
{
    setupToolbar();
    setMouseTracking(true);
    setMinimumSize(m_cols * m_cell + 16, m_rows * m_cell + m_topH + 8);
    setStatusText("画障碍: 左键画 / 右键擦除");
}

QSize MapEditorWidget::minimumSizeHint() const
{
    return QSize(m_cols * m_cell + 16, m_rows * m_cell + m_topH + 8);
}

// ========== Controller 绑定(叠加机器人与任务) ==========

void MapEditorWidget::setController(RobotController *controller)
{
    m_controller = controller;
    if (!m_controller)
        return;
    connect(m_controller, &RobotController::robotAdded, this, [this](int) { update(); });
    connect(m_controller, &RobotController::robotRemoved, this, [this](int) { update(); });
    connect(m_controller, &RobotController::robotStatusChanged, this, [this](int, RobotStatus, RobotStatus) { update(); });
    connect(m_controller, &RobotController::robotPositionChanged, this, [this](int, float, float) { update(); });
    connect(m_controller, &RobotController::taskAdded, this, [this](int) { update(); });
    connect(m_controller, &RobotController::taskRemoved, this, [this](int) { update(); });
    connect(m_controller, &RobotController::taskAssigned, this, [this](int, int) { update(); });
    connect(m_controller, &RobotController::taskFinished, this, [this](int) { update(); });
    connect(m_controller, &RobotController::taskCancelled, this, [this](int) { update(); });

    // 回原点控制与 controller 同步
    if (m_chkAutoHome)
    {
        QSignalBlocker block(m_chkAutoHome);
        m_chkAutoHome->setChecked(m_controller->isReturnHomeEnabled());
    }
    if (m_chkAutoHome)
        connect(m_chkAutoHome, &QCheckBox::toggled, this, [this](bool on)
                { m_controller->setEnableReturnHome(on); });
    update();
}

void MapEditorWidget::startNewTaskMode()
{
    m_tool = 4;
    if (m_toolCombo)
        m_toolCombo->setCurrentIndex(4);
    m_pickStart = QPoint(-1, -1);
    setStatusText("新建任务: 请点第 1 点作为任务起点");
}

bool MapEditorWidget::isObstacle(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_cols || y >= m_rows)
        return false;
    return m_grid[gridIndex(x, y)] == 1;
}

void MapEditorWidget::setObstacle(int x, int y, bool on)
{
    if (x < 0 || y < 0 || x >= m_cols || y >= m_rows)
        return;
    m_grid[gridIndex(x, y)] = on ? 1 : 0;
}

// ========== 工具栏 ==========

void MapEditorWidget::setupToolbar()
{
    int x = 6;
    m_toolCombo = new QComboBox(this);
    m_toolCombo->addItem("画障碍");
    m_toolCombo->addItem("擦除");
    m_toolCombo->addItem("设起点");
    m_toolCombo->addItem("设终点");
    m_toolCombo->addItem("新建任务");
    m_toolCombo->addItem("设充电桩");
    m_toolCombo->addItem("查看/选择");
    m_toolCombo->setFixedSize(90, 24);
    m_toolCombo->move(x, 3);
    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx)
            {
                m_tool = idx;
                m_pickStart = QPoint(-1, -1);
                if (idx == 4)
                    setStatusText("新建任务: 请点第 1 点作为任务起点");
                else if (idx == 5)
                    setStatusText("请点击地图位置放置充电桩");
                else if (idx == 6)
                    setStatusText("查看/选择: 点击机器人查看详情");
                else if (idx == 0)
                    setStatusText("画障碍: 左键画 / 右键擦除");
                else if (idx == 1)
                    setStatusText("擦除: 左键/右键擦除");
                else if (idx == 2)
                    setStatusText("请在地图上点选起点格子");
                else if (idx == 3)
                    setStatusText("请在地图上点选终点格子");
            });
    x += 98;

    m_btnNewTask = new QPushButton("新建任务", this);
    placeControl(m_btnNewTask, x);
    connect(m_btnNewTask, &QPushButton::clicked, this, &MapEditorWidget::startNewTaskMode);

    m_btnClear = new QPushButton("清空障碍", this);
    placeControl(m_btnClear, x);
    connect(m_btnClear, &QPushButton::clicked, this, [this]()
            {
        m_grid.fill(0);
        update();
        emit mapChanged();
        setStatusText("已清空障碍"); });

    m_btnSave = new QPushButton("保存地图", this);
    placeControl(m_btnSave, x);
    connect(m_btnSave, &QPushButton::clicked, this, [this]()
            {
        QString p = QFileDialog::getSaveFileName(this, "保存地图", defaultMapPath(), "地图(*.json);;所有文件(*)");
        if (p.isEmpty())
            return;
        if (saveToFile(p))
            QMessageBox::information(this, "保存", "地图已保存:\n" + p);
        else
            QMessageBox::warning(this, "保存", "保存失败"); });

    m_btnLoad = new QPushButton("导入地图", this);
    placeControl(m_btnLoad, x);
    connect(m_btnLoad, &QPushButton::clicked, this, [this]()
            {
        QString p = QFileDialog::getOpenFileName(this, "导入地图", defaultMapPath(), "地图(*.json);;所有文件(*)");
        if (p.isEmpty())
            return;
        if (!loadFromFile(p))
            QMessageBox::warning(this, "导入", "载入失败或文件无效"); });

    m_colSpin = new QSpinBox(this);
    m_colSpin->setRange(5, 200);
    m_colSpin->setValue(m_cols);
    m_colSpin->setSuffix("列");
    m_colSpin->setGeometry(x, 3, 56, 24);
    x += 62;
    m_rowSpin = new QSpinBox(this);
    m_rowSpin->setRange(5, 200);
    m_rowSpin->setValue(m_rows);
    m_rowSpin->setSuffix("行");
    m_rowSpin->setGeometry(x, 3, 56, 24);
    x += 62;
    m_btnApplySize = new QPushButton("应用尺寸", this);
    m_btnApplySize->setFixedSize(72, 24);
    m_btnApplySize->move(x, 3);
    x += 80;
    connect(m_btnApplySize, &QPushButton::clicked, this, &MapEditorWidget::applySize);

    // 回原点控制
    m_chkAutoHome = new QCheckBox("自动回原点", this);
    m_chkAutoHome->setChecked(true);
    m_chkAutoHome->setGeometry(x, 3, 96, 24);
    x += 100;
    m_btnAllHome = new QPushButton("全部回原点", this);
    m_btnAllHome->setFixedSize(90, 24);
    m_btnAllHome->move(x, 3);
    x += 98;
    connect(m_btnAllHome, &QPushButton::clicked, this, [this]()
            { if (m_controller) m_controller->returnIdleRobotsToHome(); });

    m_status = new QLabel(this);
    m_status->setGeometry(x + 8, 3, 260, 24);
    m_status->setStyleSheet("color:#444;");
}

void MapEditorWidget::placeControl(QWidget *w, int &x)
{
    w->setFixedSize(80, 24);
    w->move(x, 3);
    x += 88;
}

void MapEditorWidget::setStatusText(const QString &s)
{
    if (m_status)
        m_status->setText(s);
}

// ========== 尺寸 ==========

void MapEditorWidget::applySize()
{
    int nc = m_colSpin ? m_colSpin->value() : m_cols;
    int nr = m_rowSpin ? m_rowSpin->value() : m_rows;
    nc = qBound(5, nc, 200);
    nr = qBound(5, nr, 200);
    if (nc == m_cols && nr == m_rows)
        return;

    QVector<char> old = m_grid;
    int oc = m_cols;
    m_grid = QVector<char>(nc * nr, 0);
    for (int r = 0; r < qMin(nr, m_rows); ++r)
        for (int c = 0; c < qMin(nc, m_cols); ++c)
            m_grid[r * nc + c] = old[r * oc + c];

    m_cols = nc;
    m_rows = nr;
    if (m_start.x() >= m_cols || m_start.y() >= m_rows)
        m_start = QPoint(-1, -1);
    if (m_end.x() >= m_cols || m_end.y() >= m_rows)
        m_end = QPoint(-1, -1);
    if (m_pickStart.x() >= m_cols || m_pickStart.y() >= m_rows)
        m_pickStart = QPoint(-1, -1);

    setMinimumSize(m_cols * m_cell + 16, m_rows * m_cell + m_topH + 8);
    update();
    emit mapChanged();
}

// ========== 坐标换算(世界坐标 == 格子编号) ==========

QPoint MapEditorWidget::cellAt(const QPoint &pos) const
{
    int px = pos.x() - 8;
    int py = pos.y() - m_topH;
    if (px < 0 || py < 0)
        return QPoint(-1, -1);
    int cx = px / m_cell;
    int cy = py / m_cell;
    if (cx >= m_cols || cy >= m_rows)
        return QPoint(-1, -1);
    return QPoint(cx, cy);
}

QPointF MapEditorWidget::cellWorldCenter(const QPoint &cell) const
{
    return QPointF(cell.x() + 0.5f, cell.y() + 0.5f);
}

QPoint MapEditorWidget::cellCenterScreen(const QPoint &cell) const
{
    int px = 8 + cell.x() * m_cell + m_cell / 2;
    int py = m_topH + cell.y() * m_cell + m_cell / 2;
    return QPoint(px, py);
}

void MapEditorWidget::setEditable(bool editable)
{
    m_editable = editable;
    if (!editable && (m_tool != 4))
    {
        // 非编辑(普通)用户只能新建任务/查看
        m_tool = 4;
        if (m_toolCombo)
            m_toolCombo->setCurrentIndex(4);
        setStatusText("只读+新建任务模式");
    }
}

bool MapEditorWidget::robotAtWorld(float wx, float wy, int &outId) const
{
    if (!m_controller)
        return false;
    int best = -1;
    float bestD = 1e9f;
    for (int id : m_controller->getAllRobotIds())
    {
        const Robot *r = m_controller->getRobot(id);
        if (!r)
            continue;
        float d = std::sqrt((r->getPx() - wx) * (r->getPx() - wx) +
                            (r->getPy() - wy) * (r->getPy() - wy));
        if (d < bestD)
        {
            bestD = d;
            best = id;
        }
    }
    if (best < 0 || bestD > 0.6f)
        return false;
    outId = best;
    return true;
}

// ========== 操作 ==========

void MapEditorWidget::applyTool(const QPoint &cell)
{
    if (cell.x() < 0)
        return;

    if (m_tool == 6) // 查看/选择：显示机器人详情
    {
        QPointF w = cellWorldCenter(cell);
        int rid = -1;
        if (robotAtWorld((float)w.x(), (float)w.y(), rid))
        {
            const Robot *r = m_controller ? m_controller->getRobot(rid) : nullptr;
            if (r)
                setStatusText(QString("机器人 R%1 | 状态:%2 | 位置:(%3,%4) | 电量:%5% | 速度:%6 | 任务:%7")
                                  .arg(rid)
                                  .arg(robotStatusName((int)r->getStatus()))
                                  .arg(r->getPx(), 0, 'f', 1)
                                  .arg(r->getPy(), 0, 'f', 1)
                                  .arg(r->getBattery())
                                  .arg(r->getSpeed(), 0, 'f', 1)
                                  .arg(r->getTask()));
        }
        else
            setStatusText("该位置没有机器人");
        return;
    }

    if (m_tool == 4) // 新建任务：点两点定起终点(普通用户也允许)
    {
        if (m_pickStart.x() < 0)
        {
            m_pickStart = cell;
            setStatusText("已选起点(" + QString::number(cell.x()) + "," + QString::number(cell.y()) +
                          ")，请再点一点作为任务终点");
            update();
        }
        else
        {
            if (cell == m_pickStart)
            {
                setStatusText("起点与终点相同，请重选终点");
                return;
            }
            QPointF s = cellWorldCenter(m_pickStart);
            QPointF e = cellWorldCenter(cell);
            m_pickStart = QPoint(-1, -1);
            emit requestAddTask(s, e);
            setStatusText("任务已提交");
            update();
        }
        return;
    }

    // 非编辑用户(普通)不能画障碍/改点/放充电桩
    if (!m_editable)
        return;

    int idx = gridIndex(cell.x(), cell.y());
    switch (m_tool)
    {
    case 0: // 画障碍
        if (cell != m_start && cell != m_end)
            m_grid[idx] = 1;
        break;
    case 1: // 擦除
        m_grid[idx] = 0;
        break;
    case 2: // 设地图起点(展示标记)
        m_start = cell;
        m_grid[idx] = 0;
        break;
    case 3: // 设地图终点(展示标记)
        m_end = cell;
        m_grid[idx] = 0;
        break;
    case 5: // 设充电桩
        if (m_controller)
        {
            QPointF w = cellWorldCenter(cell);
            m_controller->addCharger((float)w.x(), (float)w.y());
            setStatusText("已放置充电桩");
        }
        break;
    default:
        break;
    }
    update();
    emit mapChanged();
}

void MapEditorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = true;
        applyTool(cellAt(event->pos()));
        event->accept();
    }
    else if (event->button() == Qt::RightButton)
    {
        m_dragging = true;
        QPoint c = cellAt(event->pos());
        if (c.x() >= 0)
        {
            m_grid[gridIndex(c.x(), c.y())] = 0;
            update();
            emit mapChanged();
        }
        event->accept();
    }
    else
        QWidget::mousePressEvent(event);
}

void MapEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (m_tool == 0 || m_tool == 1))
    {
        QPoint cell = cellAt(event->pos());
        if (cell.x() >= 0)
        {
            m_grid[gridIndex(cell.x(), cell.y())] = (m_tool == 0) ? 1 : 0;
            update();
            emit mapChanged();
        }
        event->accept();
    }
}

void MapEditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
    {
        m_dragging = false;
        event->accept();
    }
    else
        QWidget::mouseReleaseEvent(event);
}

// 滚轮缩放(以鼠标位置为中心尽量保持所见区域)
void MapEditorWidget::zoomAt(int delta, const QPoint &pos)
{
    Q_UNUSED(pos); // 简单起见先整图缩放；拖拽平移待后续
    int newCell = delta > 0 ? m_cell + 2 : m_cell - 2;
    newCell = qBound(6, newCell, 40);
    if (newCell == m_cell)
        return;
    m_cell = newCell;
    setMinimumSize(m_cols * m_cell + 16, m_rows * m_cell + m_topH + 8);
    update();
    setStatusText(QString("缩放: %1 px/格 (滚轮缩放)").arg(m_cell));
}

void MapEditorWidget::wheelEvent(QWheelEvent *event)
{
    zoomAt(event->angleDelta().y(), event->pos());
    event->accept();
}

// ========== 绘制 ==========

static QString robotStatusName(int status)
{
    switch (static_cast<RobotStatus>(status))
    {
    case RobotStatus::Idle: return "空闲";
    case RobotStatus::Busy: return "执行中";
    case RobotStatus::Error: return "故障";
    case RobotStatus::Offline: return "离线";
    case RobotStatus::Charging: return "充电";
    case RobotStatus::Lowbattery: return "低电量";
    default: return "未知";
    }
}

static QColor robotColor(int status)
{
    (void)robotStatusName; // keep used

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

void MapEditorWidget::drawOverlay(QPainter &p)
{
    if (!m_controller)
        return;
    const int left = 8;
    const int top = m_topH;
    auto toScr = [&](float wx, float wy) -> QPointF
    {
        return QPointF(left + wx * m_cell, top + wy * m_cell);
    };

    // 任务配色(按任务ID取色)
    auto taskColor = [](int id)
    {
        static const QColor pal[] = {QColor("#1976d2"), QColor("#388e3c"), QColor("#e65100"),
                                     QColor("#6a1b9a"), QColor("#00838f"), QColor("#c2185b")};
        return pal[((unsigned)id) % (sizeof(pal) / sizeof(pal[0]))];
    };

    // 任务：路径(按任务配色、过格子中线) + 起终点
    for (int id : m_controller->getAllTaskIds())
    {
        const Task *t = m_controller->getTask(id);
        if (!t)
            continue;
        QColor col = taskColor(id);
        QList<QPointF> path = m_controller->planPathWorld(t->getStartX(), t->getStartY(),
                                                          t->getEndX(), t->getEndY());
        if (path.size() >= 2)
        {
            QPen pen(col, 2);
            p.setPen(pen);
            QPointF prev = toScr((float)path[0].x(), (float)path[0].y());
            for (int i = 1; i < path.size(); ++i)
            {
                QPointF c = toScr((float)path[i].x(), (float)path[i].y());
                p.drawLine(prev, c);
                prev = c;
            }
        }
        // 起点/终点标记
        QPointF s = toScr(t->getStartX(), t->getStartY());
        QPointF e = toScr(t->getEndX(), t->getEndY());
        p.setPen(QPen(QColor("white"), 1));
        p.setBrush(QColor("#1a7f37"));
        p.drawEllipse(s, 5, 5);
        p.setBrush(QColor("#d92332"));
        p.drawEllipse(e, 5, 5);
        p.setPen(col);
        p.drawText(QPointF(e.x() + 6, e.y() - 6), QString::number(id));
    }

    // 充电桩
    p.setFont(QFont("Sans", 7));
    for (const QPointF &ch : m_controller->chargers())
    {
        QPointF c = toScr((float)ch.x(), (float)ch.y());
        p.setPen(QPen(QColor("white"), 2));
        p.setBrush(QColor("#7b1fa2"));
        p.drawEllipse(c, m_cell / 2 - 1, m_cell / 2 - 1);
        p.setPen(QColor("#ffffff"));
        p.drawText(QPointF(c.x() - m_cell / 2, c.y() + m_cell / 2 - 2), QStringLiteral("⚡"));
    }

    // 机器人
    for (int id : m_controller->getAllRobotIds())
    {
        const Robot *r = m_controller->getRobot(id);
        if (!r)
            continue;
        QPointF c = toScr(r->getPx(), r->getPy());
        QColor col = robotColor(static_cast<int>(r->getStatus()));

        // 碰撞体积(约一格)：虚线方格占位
        QRectF fp(c.x() - m_cell / 2.0, c.y() - m_cell / 2.0, m_cell, m_cell);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 60), 1, Qt::DashLine));
        p.drawRect(fp);

        p.setPen(QPen(QColor("white"), 2));
        p.setBrush(col);
        p.drawEllipse(c, m_cell / 2 - 2, m_cell / 2 - 2);
        p.setPen(col.darker(160));
        p.setFont(QFont("Sans", 8, QFont::Bold));
        p.drawText(QPointF(c.x() + m_cell / 2 + 1, c.y() - m_cell / 2 + 2),
                   QString("R%1").arg(id));
        // 电量百分比(小字)
        p.setFont(QFont("Sans", 7));
        p.setPen(QColor("#333333"));
        p.drawText(QPointF(c.x() - m_cell / 2, c.y() + m_cell / 2 + 2),
                   QString("%1%").arg(r->getBattery()));
    }
}

void MapEditorWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int left = 8;
    const int top = m_topH;

    p.fillRect(rect(), QColor("#eef2f5"));
    p.fillRect(left, top, m_cols * m_cell, m_rows * m_cell, QColor("#f7f9fb"));

    for (int y = 0; y < m_rows; ++y)
    {
        for (int x = 0; x < m_cols; ++x)
        {
            QRect rc(left + x * m_cell, top + y * m_cell, m_cell, m_cell);
            if (m_grid[gridIndex(x, y)] == 1)
                p.fillRect(rc, QColor("#546e7a"));
            else if ((x + y) % 2 == 0)
                p.fillRect(rc, QColor("#eef2f5"));
        }
    }

    p.setPen(QPen(QColor(205, 213, 220), 1));
    for (int x = 0; x <= m_cols; ++x)
        p.drawLine(left + x * m_cell, top, left + x * m_cell, top + m_rows * m_cell);
    for (int y = 0; y <= m_rows; ++y)
        p.drawLine(left, top + y * m_cell, left + m_cols * m_cell, top + y * m_cell);

    // 地图标记起点/终点
    if (m_start.x() >= 0)
    {
        QPoint c = cellCenterScreen(m_start);
        QRectF tr(c.x() - m_cell / 2, c.y() - m_cell / 2, m_cell, m_cell);
        p.setBrush(QColor("#1a7f37"));
        p.setPen(QPen(QColor("white"), 2));
        p.drawEllipse(tr);
        p.drawText(tr, Qt::AlignCenter, QStringLiteral("S"));
    }
    if (m_end.x() >= 0)
    {
        QPoint c = cellCenterScreen(m_end);
        QRectF tr(c.x() - m_cell / 2, c.y() - m_cell / 2, m_cell, m_cell);
        p.setBrush(QColor("#d92332"));
        p.setPen(QPen(QColor("white"), 2));
        p.drawEllipse(tr);
        p.drawText(tr, Qt::AlignCenter, QStringLiteral("E"));
    }

    // 新建任务已选的第 1 点
    if (m_tool == 4 && m_pickStart.x() >= 0)
    {
        QPoint c = cellCenterScreen(m_pickStart);
        QRectF tr(c.x() - m_cell / 2, c.y() - m_cell / 2, m_cell, m_cell);
        p.setPen(QPen(QColor("#1a7f37"), 2, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(tr);
    }

    drawOverlay(p); // 实时机器人 + 任务
}

// ========== 存/载 ==========

void MapEditorWidget::clearMap()
{
    m_grid.fill(0);
    m_start = QPoint(-1, -1);
    m_end = QPoint(-1, -1);
    update();
    emit mapChanged();
}

bool MapEditorWidget::saveToFile(const QString &path)
{
    QString filePath = path.isEmpty() ? defaultMapPath() : path;
    QJsonObject obj;
    obj["cols"] = m_cols;
    obj["rows"] = m_rows;
    QJsonArray obstacles;
    for (int y = 0; y < m_rows; ++y)
        for (int x = 0; x < m_cols; ++x)
            if (m_grid[gridIndex(x, y)] == 1)
            {
                QJsonArray c;
                c.append(x);
                c.append(y);
                obstacles.append(c);
            }
    obj["obstacles"] = obstacles;
    if (m_start.x() >= 0)
    {
        QJsonArray c;
        c.append(m_start.x());
        c.append(m_start.y());
        obj["start"] = c;
    }
    if (m_end.x() >= 0)
    {
        QJsonArray c;
        c.append(m_end.x());
        c.append(m_end.y());
        obj["end"] = c;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool MapEditorWidget::loadFromFile(const QString &path)
{
    QString filePath = path.isEmpty() ? defaultMapPath() : path;
    if (!QFile::exists(filePath))
        return false;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    QJsonObject obj = doc.object();

    int fc = qBound(5, obj.value("cols").toInt(m_cols), 200);
    int fr = qBound(5, obj.value("rows").toInt(m_rows), 200);
    m_grid = QVector<char>(fc * fr, 0);
    m_cols = fc;
    m_rows = fr;
    if (m_colSpin)
        m_colSpin->setValue(m_cols);
    if (m_rowSpin)
        m_rowSpin->setValue(m_rows);
    m_start = QPoint(-1, -1);
    m_end = QPoint(-1, -1);
    m_pickStart = QPoint(-1, -1);

    for (const QJsonValue &v : obj["obstacles"].toArray())
    {
        QJsonArray c = v.toArray();
        int x = c[0].toInt(-1);
        int y = c[1].toInt(-1);
        if (x >= 0 && y >= 0 && x < m_cols && y < m_rows)
            m_grid[gridIndex(x, y)] = 1;
    }
    if (obj["start"].isArray())
    {
        QJsonArray c = obj["start"].toArray();
        int sx = c[0].toInt(-1), sy = c[1].toInt(-1);
        if (sx >= 0 && sy >= 0 && sx < m_cols && sy < m_rows)
            m_start = QPoint(sx, sy);
    }
    if (obj["end"].isArray())
    {
        QJsonArray c = obj["end"].toArray();
        int ex = c[0].toInt(-1), ey = c[1].toInt(-1);
        if (ex >= 0 && ey >= 0 && ex < m_cols && ey < m_rows)
            m_end = QPoint(ex, ey);
    }
    setMinimumSize(m_cols * m_cell + 16, m_rows * m_cell + m_topH + 8);
    update();
    emit mapChanged();
    return true;
}
