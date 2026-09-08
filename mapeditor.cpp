#include "mapeditor.h"

#include <QPainter>
#include <QMouseEvent>
#include <QGuiApplication>
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
    , m_cell(16)
    , m_grid(m_cols * m_rows, 0)
    , m_start(-1, -1)
    , m_end(-1, -1)
    , m_tool(0)
    , m_dragging(false)
    , m_toolCombo(nullptr)
    , m_btnClear(nullptr)
    , m_btnSave(nullptr)
    , m_btnLoad(nullptr)
    , m_status(nullptr)
    , m_topH(32)
{
    setupToolbar();
    setMouseTracking(true);
    setMinimumSize(m_cols * m_cell + 16, m_rows * m_cell + m_topH + 8);
    m_status->setText("画障碍: 左键画 / 右键擦除");
}

QSize MapEditorWidget::minimumSizeHint() const
{
    return QSize(m_cols * m_cell + 16, m_rows * m_cell + m_topH + 8);
}

bool MapEditorWidget::isObstacle(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_cols || y >= m_rows)
        return false;
    return m_grid[y * m_cols + x] == 1;
}

void MapEditorWidget::setObstacle(int x, int y, bool on)
{
    if (x < 0 || y < 0 || x >= m_cols || y >= m_rows)
        return;
    m_grid[y * m_cols + x] = on ? 1 : 0;
}

void MapEditorWidget::setupToolbar()
{
    // 手动排版顶部工具条（避免复杂布局，网格从 m_topH 之下开始画）
    int x = 6;
    m_toolCombo = new QComboBox(this);
    m_toolCombo->addItem("画障碍");
    m_toolCombo->addItem("擦除");
    m_toolCombo->addItem("设起点");
    m_toolCombo->addItem("设终点");
    m_toolCombo->setFixedWidth(90);
    m_toolCombo->move(x, 4);
    m_toolCombo->resize(90, 24);
    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) { m_tool = idx; });
    x += 98;

    m_btnClear = new QPushButton("全部清空", this);
    placeControl(m_btnClear, x);
    connect(m_btnClear, &QPushButton::clicked, this, &MapEditorWidget::clearMap);

    m_btnSave = new QPushButton("保存地图", this);
    placeControl(m_btnSave, x);
    connect(m_btnSave, &QPushButton::clicked, this, [this]()
            {
        QString p = QFileDialog::getSaveFileName(this, "保存地图", defaultMapPath(), "地图(*.json);;所有文件(*)");
        if (!p.isEmpty())
        {
            if (saveToFile(p))
                QMessageBox::information(this, "保存", "地图已保存:\n" + p);
            else
                QMessageBox::warning(this, "保存", "保存失败");
        } });

    m_btnLoad = new QPushButton("导入地图", this);
    placeControl(m_btnLoad, x);
    connect(m_btnLoad, &QPushButton::clicked, this, [this]()
            {
        QString p = QFileDialog::getOpenFileName(this, "导入地图", defaultMapPath(), "地图(*.json);;所有文件(*)");
        if (!p.isEmpty())
        {
            if (!loadFromFile(p))
                QMessageBox::warning(this, "导入", "载入失败或文件无效");
        } });

    // 尺寸：列 x 行 + 应用
    m_colSpin = new QSpinBox(this);
    m_colSpin->setRange(5, 200);
    m_colSpin->setValue(m_cols);
    m_colSpin->setSuffix(" 列");
    m_colSpin->setGeometry(x, 4, 62, 24);
    x += 68;
    m_rowSpin = new QSpinBox(this);
    m_rowSpin->setRange(5, 200);
    m_rowSpin->setValue(m_rows);
    m_rowSpin->setSuffix(" 行");
    m_rowSpin->setGeometry(x, 4, 62, 24);
    x += 68;
    m_btnApplySize = new QPushButton("应用尺寸", this);
    m_btnApplySize->setFixedSize(76, 24);
    m_btnApplySize->move(x, 4);
    x += 84;
    connect(m_btnApplySize, &QPushButton::clicked, this, &MapEditorWidget::applySize);

    m_status = new QLabel(this);
    m_status->setGeometry(x + 10, 4, 260, 24);
}

void MapEditorWidget::placeControl(QWidget *w, int &x)
{
    w->setFixedSize(80, 24);
    w->move(x, 4);
    x += 88;
}

void MapEditorWidget::clearMap()
{
    m_grid.fill(0);
    m_start = QPoint(-1, -1);
    m_end = QPoint(-1, -1);
    update();
    emit mapChanged();
}

void MapEditorWidget::applySize()
{
    int nc = m_colSpin ? m_colSpin->value() : m_cols;
    int nr = m_rowSpin ? m_rowSpin->value() : m_rows;
    nc = qBound(5, nc, 200);
    nr = qBound(5, nr, 200);
    if (nc == m_cols && nr == m_rows)
        return;

    // 保留重叠区域内的障碍与起终点，越界部分丢弃
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

    setMinimumSize(m_cols * m_cell + 16, m_rows * m_cell + m_topH + 8);
    update();
    emit mapChanged();
}

QPoint MapEditorWidget::cellAt(const QPoint &pos) const
{
    int px = pos.x() - 8; // 左留白
    int py = pos.y() - m_topH;
    if (px < 0 || py < 0)
        return QPoint(-1, -1);
    int cx = px / m_cell;
    int cy = py / m_cell;
    if (cx >= m_cols || cy >= m_rows)
        return QPoint(-1, -1);
    return QPoint(cx, cy);
}

QPoint MapEditorWidget::cellCenter(const QPoint &cell) const
{
    int px = 8 + cell.x() * m_cell + m_cell / 2;
    int py = m_topH + cell.y() * m_cell + m_cell / 2;
    return QPoint(px, py);
}

void MapEditorWidget::applyTool(const QPoint &cell, bool eraseOverride)
{
    if (cell.x() < 0)
        return;
    int idx = cell.y() * m_cols + cell.x();

    if (eraseOverride || m_tool == 1)
    {
        m_grid[idx] = 0;
        update();
        emit mapChanged();
        return;
    }

    switch (m_tool)
    {
    case 0: // 画障碍
        // 起点/终点格子上不画障碍
        if (cell != m_start && cell != m_end)
            m_grid[idx] = 1;
        break;
    case 2: // 设起点
        m_start = cell;
        m_grid[idx] = 0;
        break;
    case 3: // 设终点
        m_end = cell;
        m_grid[idx] = 0;
        break;
    default:
        break;
    }
    update();
    emit mapChanged();
}

void MapEditorWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int left = 8;
    const int top = m_topH;

    // 背景(网格可绘制区)
    p.fillRect(left, top, m_cols * m_cell, m_rows * m_cell, QColor("#f7f9fb"));

    // 格子
    for (int y = 0; y < m_rows; ++y)
    {
        for (int x = 0; x < m_cols; ++x)
        {
            QRect rc(left + x * m_cell, top + y * m_cell, m_cell, m_cell);
            if (m_grid[y * m_cols + x] == 1)
            {
                p.fillRect(rc, QColor("#546e7a"));
            }
            else if ((x + y) % 2 == 0)
            {
                p.fillRect(rc, QColor("#f1f5f8")); // 棋盘底色方便数格子
            }
        }
    }

    // 网格线
    p.setPen(QPen(QColor(200, 208, 214), 1));
    for (int x = 0; x <= m_cols; ++x)
        p.drawLine(left + x * m_cell, top, left + x * m_cell, top + m_rows * m_cell);
    for (int y = 0; y <= m_rows; ++y)
        p.drawLine(left, top + y * m_cell, left + m_cols * m_cell, top + y * m_cell);

    // 起点/终点
    if (m_start.x() >= 0)
    {
        QPoint c = cellCenter(m_start);
        QRectF tr(c.x() - m_cell / 2, c.y() - m_cell / 2, m_cell, m_cell);
        p.setPen(QPen(QColor("white"), 2));
        p.setBrush(QColor("#1a7f37"));
        p.drawEllipse(tr);
        p.drawText(tr, Qt::AlignCenter, QStringLiteral("S"));
    }
    if (m_end.x() >= 0)
    {
        QPoint c = cellCenter(m_end);
        QRectF tr(c.x() - m_cell / 2, c.y() - m_cell / 2, m_cell, m_cell);
        p.setPen(QPen(QColor("white"), 2));
        p.setBrush(QColor("#d92332"));
        p.drawEllipse(tr);
        p.drawText(tr, Qt::AlignCenter, QStringLiteral("E"));
    }
}

void MapEditorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = true;
        applyTool(cellAt(event->pos()), false);
        event->accept();
    }
    else if (event->button() == Qt::RightButton)
    {
        m_dragging = true;
        applyTool(cellAt(event->pos()), true); // 右键=擦除
        event->accept();
    }
    else
        QWidget::mousePressEvent(event);
}

void MapEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging)
    {
        QPoint cell = cellAt(event->pos());
        if (cell.x() >= 0)
        {
            applyTool(cell, QGuiApplication::mouseButtons() & Qt::RightButton);
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

bool MapEditorWidget::saveToFile(const QString &path)
{
    QString filePath = path.isEmpty() ? defaultMapPath() : path;

    QJsonObject obj;
    obj["cols"] = m_cols;
    obj["rows"] = m_rows;
    QJsonArray obstacles;
    for (int y = 0; y < m_rows; ++y)
        for (int x = 0; x < m_cols; ++x)
            if (m_grid[y * m_cols + x] == 1)
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

    // 依据文件记录调整尺寸(自动裁剪到 [5,200])
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

    for (const QJsonValue &v : obj["obstacles"].toArray())
    {
        QJsonArray c = v.toArray();
        int x = c[0].toInt(-1);
        int y = c[1].toInt(-1);
        if (x >= 0 && y >= 0 && x < m_cols && y < m_rows)
            m_grid[y * m_cols + x] = 1;
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
