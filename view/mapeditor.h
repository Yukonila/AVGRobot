#ifndef MAPEDITOR_H
#define MAPEDITOR_H

#include <QWidget>
#include <QPoint>
#include <QPointF>
#include <QVector>

class QComboBox;
class QPushButton;
class QLabel;
class QSpinBox;
class QCheckBox;
class QWheelEvent;
class RobotController;

// ============ 合一画布（地图编辑 + 机器人/任务监控 + 点画布新建任务） ============
// 网格：默认 1 格 = 1 世界单位。工具栏模式：
//   画障碍 / 擦除 / 设地图起点 / 设地图终点 / 新建任务(点两点分别定起点/终点)
class MapEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapEditorWidget(QWidget *parent = nullptr);
    ~MapEditorWidget() override = default;

    QSize minimumSizeHint() const override;

    // 绑定实时数据源，在画布上叠加机器人与任务
    void setController(RobotController *controller);
    RobotController *controller() const { return m_controller; }

    // 数据访问
    int columns() const { return m_cols; }
    int rows() const { return m_rows; }
    bool isObstacle(int x, int y) const;
    void setObstacle(int x, int y, bool on = true);
    QPoint start() const { return m_start; }
    QPoint end() const { return m_end; }
    QVector<char> obstacleGrid() const { return m_grid; }

    // 保存/载入；path 为空则用默认 <运行目录>/Data/map.json
    bool saveToFile(const QString &path);
    bool loadFromFile(const QString &path);
    static QString defaultMapPath();
    void clearMap();

    // 切到“新建任务”模式：在画布上依次点起点、终点
    void startNewTaskMode();
    // 是否允许编辑地图(管理员=true；普通用户仅新建任务/查看=false)
    void setEditable(bool editable);
    bool isEditable() const { return m_editable; }

signals:
    void mapChanged();
    // 用户点好起点/终点后请求创建任务(格子中心的世界坐标)
    void requestAddTask(const QPointF &start, const QPointF &end);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void setupToolbar();
    void applyTool(const QPoint &cell);
    void applySize();
    void zoomAt(int delta, const QPoint &pos); // 滚轮缩放
    QPoint cellAt(const QPoint &pos) const;      // 屏幕->格子
    QPointF cellWorldCenter(const QPoint &cell) const; // 格子中心世界坐标
    QPoint cellCenterScreen(const QPoint &cell) const; // 格子中心(屏幕)
    void placeControl(QWidget *w, int &x);
    void drawOverlay(QPainter &p);   // 画机器人/任务叠加
    void setStatusText(const QString &s);
    int gridIndex(int x, int y) const { return y * m_cols + x; }
    bool robotAtWorld(float wx, float wy, int &outId) const; // 命中机器人

    // 网格数据
    int m_cols;
    int m_rows;
    int m_cell;
    QVector<char> m_grid; // 0=空闲, 1=障碍
    QPoint m_start;       // 展示用地图起点格子
    QPoint m_end;         // 展示用地图终点格子

    // 工具：0画障碍 1擦除 2设起点 3设终点 4新建任务 5设充电桩
    int m_tool;
    bool m_dragging;
    bool m_editable = true;

    RobotController *m_controller;
    QPoint m_pickStart; // 新建任务模式已选的第1点(-1,-1 表示未选)

    QComboBox *m_toolCombo;
    QPushButton *m_btnClear;
    QPushButton *m_btnSave;
    QPushButton *m_btnLoad;
    QPushButton *m_btnNewTask;
    QSpinBox *m_colSpin;
    QSpinBox *m_rowSpin;
    QPushButton *m_btnApplySize;
    QCheckBox *m_chkAutoHome;   // 自动回原点
    QPushButton *m_btnAllHome;  // 全部回原点
    QLabel *m_status;

    int m_topH; // 顶部工具栏占用的高度
};

#endif // MAPEDITOR_H
