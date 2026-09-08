#ifndef MAPEDITOR_H
#define MAPEDITOR_H

#include <QWidget>
#include <QPoint>
#include <QVector>

class QComboBox;
class QPushButton;
class QLabel;
class QSpinBox;

// ============ 网格地图编辑器 ============
// 左键按当前工具操作: 画障碍 / 擦除 / 设起点 / 设终点；右键=擦除。
// 支持调整网格尺寸(列/行)与用 JSON 文件保存/导入(默认 <运行目录>/Data/map.json)。
class MapEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapEditorWidget(QWidget *parent = nullptr);
    ~MapEditorWidget() override = default;

    QSize minimumSizeHint() const override;

    // 数据访问(供后续 A*、地图监控叠加障碍等使用)
    int columns() const { return m_cols; }
    int rows() const { return m_rows; }
    bool isObstacle(int x, int y) const;
    void setObstacle(int x, int y, bool on = true);
    QPoint start() const { return m_start; }
    QPoint end() const { return m_end; }

    // 保存/载入；path 为空则用默认 <运行目录>/Data/map.json
    bool saveToFile(const QString &path);
    bool loadFromFile(const QString &path);
    static QString defaultMapPath();
    void clearMap();

signals:
    // 地图内容/尺寸变化，供“地图监控”等联动刷新
    void mapChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupToolbar();
    void applyTool(const QPoint &cell, bool eraseOverride = false);
    void applySize();
    QPoint cellAt(const QPoint &pos) const;    // 屏幕->格子
    QPoint cellCenter(const QPoint &cell) const; // 格子中心(屏幕)
    void placeControl(QWidget *w, int &x);

    // 网格数据
    int m_cols;
    int m_rows;
    int m_cell;
    QVector<char> m_grid;   // 0=空闲, 1=障碍
    QPoint m_start;         // 起点格子
    QPoint m_end;           // 终点格子

    // 工具：0画障碍 1擦除 2设起点 3设终点
    int m_tool;
    bool m_dragging;

    QComboBox *m_toolCombo;
    QPushButton *m_btnClear;
    QPushButton *m_btnSave;
    QPushButton *m_btnLoad;
    QSpinBox *m_colSpin;
    QSpinBox *m_rowSpin;
    QPushButton *m_btnApplySize;
    QLabel *m_status;

    int m_topH; // 顶部工具栏占用的高度，绘图从它下方开始
};

#endif // MAPEDITOR_H
