#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QRectF>
#include <QColor>
#include <vector>
#include <QVector>
#include <QPair>

/*
  TreeMapWidget
  - Squarified treemap layout
  - Area-proportional visibility threshold (relative to absolute root size)
  - Dynamic max children by depth (8,6,4,2,0)
  - Clubbing beyond depth limit into a virtual "+ [X Files/Folders] (Y%)" node
  - Adaptive label suppression (draw truncated label with minimum 3 characters)
  - Legend at bottom (5 levels)

  Added interactive features:
  - Click a rectangle to make that item the visualization root (no rescanning)
  - Click on clubbed nodes to expand and navigate into them
  - Hover any rectangle to display a tooltip with FileNode details
*/

class TreeMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit TreeMapWidget(QWidget *parent = nullptr);

    // set the QTreeWidget source and maximum recursion depth
    void setSourceTree(QTreeWidget *tree, int maxDepth = 5);
    void clearSource();

    // change max visible at root (top-level). Depth-based allowed children computed automatically.
    void setMaxVisibleAtRoot(int n) { m_maxVisibleRoot = n; update(); }

    // adjust area-proportional visibility threshold (percent of absolute root size)
    void setMinRootPercent(double pct) { m_minRootPercent = pct; update(); }

    // programmatically set/clear the current visualization root
    void setCurrentRoot(QTreeWidgetItem *item);
    void clearCurrentRoot();

signals:
    // emitted when user clicks to change the root (or programmatically via setCurrentRoot)
    void rootChanged(QTreeWidgetItem *newRoot);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QTreeWidget *m_tree = nullptr;
    int m_maxDepth = 5;

    // interactive state
    QTreeWidgetItem *m_currentRoot = nullptr; // if null, top-level item(0) is used

    // **CHANGED: Store both item pointer AND list of clubbed items for virtual nodes**
    struct RectMapEntry {
        QRectF rect;
        QTreeWidgetItem *item;  // nullptr for virtual club nodes
        QVector<QTreeWidgetItem*> clubbedItems;  // populated for virtual nodes
    };
    QVector<RectMapEntry> m_rectMap;

    QTreeWidgetItem *m_hoveredItem = nullptr;

    // appearance
    const qreal m_margin = 6.0;           // outer margin
    const QColor m_borderColor = QColor("#ADCDED");

    // color gradient (levels 0..4)
    QColor m_depthColors[5] = {
        QColor("#317cf1"), QColor("#408cf1"), QColor("#519bf0"),
        QColor("#61acee"), QColor("#71bdf0")
    };

    // configuration
    int m_maxVisibleRoot = 8;     // visible children at depth 0 (reduced by 2 per depth)
    double m_minRootPercent = 0.2; // minimum % (of absolute root size) for a node to be shown (default 0.2%)

    // helper / modular methods
    quint64 itemRawSize(QTreeWidgetItem *item) const;
    int allowedChildrenForDepth(int depth) const;
    bool passesRootVisibility(QTreeWidgetItem *item, quint64 absoluteRootSize) const;
    int getItemDepth(QTreeWidgetItem *item) const;
    QString humanReadableSize(quint64 bytes) const;

    struct ChildDesc {
        QString name;
        quint64 size;
        QTreeWidgetItem *ptr; // null for virtual (clubbed) nodes
        bool isVirtual = false;
        QVector<QTreeWidgetItem*> clubbedItems;  // **NEW: store clubbed items**
    };

    // Build and filter children (applies root-percent filter). Returns sorted vector descending size.
    std::vector<ChildDesc> buildFilteredChildren(QTreeWidgetItem *parent, quint64 absoluteRootSize) const;

    // Apply per-depth max children & club remainder into one virtual node
    void applyDepthClubbing(std::vector<ChildDesc> &children, int maxVisible) const;

    // Squarified treemap algorithm
    // Input: list of sizes (in same order as children) and the rectangle to fill
    // Output: vector of QRectF corresponding to each child (same order)
    std::vector<QRectF> squarifyLayout(const std::vector<quint64> &sizes, const QRectF &rect) const;

    // drawing helpers
    void drawItem(QPainter &painter, const QRectF &rect, QTreeWidgetItem *item, int depth,
                  quint64 absoluteRootSize);
    void drawVirtualItem(QPainter &painter, const QRectF &rect, const ChildDesc &virtualChild,
                         QTreeWidgetItem *parentItem);
    void drawLegend(QPainter &painter, const QRectF &rect);

    // **NEW: Helper to show clubbed items menu**
    void showClubbedItemsMenu(const QVector<QTreeWidgetItem*> &items, const QPoint &globalPos);
};
