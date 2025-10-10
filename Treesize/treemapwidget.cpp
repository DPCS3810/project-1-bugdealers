//treemapwidget.cpp

#include "treemapwidget.h"
#include <QPainter>
#include <QDebug>
#include <algorithm>
#include <vector>
#include <QFontMetrics>
#include <numeric>
#include <limits>

TreeMapWidget::TreeMapWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(240, 180);
}

void TreeMapWidget::setSourceTree(QTreeWidget *tree, int maxDepth)
{
    m_tree = tree;
    m_maxDepth = maxDepth;
    update();
}

void TreeMapWidget::clearSource()
{
    m_tree = nullptr;
    update();
}

quint64 TreeMapWidget::itemRawSize(QTreeWidgetItem *item) const
{
    if (!item) return 0;
    QVariant v = item->data(1, Qt::UserRole);
    if (v.isValid()) {
        bool ok = false;
        quint64 val = v.toString().toULongLong(&ok);
        if (ok) return val;
    }
    // fallback parse
    QString text = item->text(1).trimmed();
    QString numberPart = text.split(' ').first().replace(",", "");
    bool ok = false;
    quint64 fallback = numberPart.toULongLong(&ok);
    if (ok) return fallback;
    return 0;
}

int TreeMapWidget::allowedChildrenForDepth(int depth) const
{
    // Start with m_maxVisibleRoot at depth 0, reduce by 2 per level
    int n = m_maxVisibleRoot - depth * 2;
    return std::max(0, n);
}

bool TreeMapWidget::passesRootVisibility(QTreeWidgetItem *item, quint64 absoluteRootSize) const
{
    if (!item || absoluteRootSize == 0) return false;
    double pct = (100.0 * double(itemRawSize(item))) / double(absoluteRootSize);
    return pct >= m_minRootPercent;
}

std::vector<TreeMapWidget::ChildDesc> TreeMapWidget::buildFilteredChildren(QTreeWidgetItem *parent, quint64 absoluteRootSize) const
{
    std::vector<ChildDesc> out;
    if (!parent) return out;

    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *c = parent->child(i);
        quint64 s = itemRawSize(c);
        // filter by absolute root percent
        if (absoluteRootSize > 0 && !passesRootVisibility(c, absoluteRootSize)) {
            continue; // skip very small items relative to absolute root
        }
        ChildDesc d;
        d.name = c->text(0);
        d.size = s;
        d.ptr = c;
        d.isVirtual = false;
        out.push_back(d);
    }

    // sort descending by size
    std::sort(out.begin(), out.end(), [](const ChildDesc &a, const ChildDesc &b) {
        return a.size > b.size;
    });
    return out;
}

void TreeMapWidget::applyDepthClubbing(std::vector<ChildDesc> &children, int maxVisible) const
{
    if (maxVisible <= 0) {
        // show none as direct children -> club all into one
        quint64 total = 0;
        for (auto &c : children) total += c.size;
        children.clear();
        if (total > 0) {
            ChildDesc club;
            club.name = QString("➕ [%1 Files/Folders]").arg(0); // we'll set count below
            club.size = total;
            club.ptr = nullptr;
            club.isVirtual = true;
            children.push_back(club);
        }
        return;
    }

    if ((int)children.size() <= maxVisible) return;

    // compute others sum for beyond maxVisible
    int totalChildren = (int)children.size();
    quint64 othersSum = 0;
    for (int i = maxVisible; i < totalChildren; ++i) othersSum += children[i].size;

    // keep top maxVisible and append virtual node
    int kept = maxVisible;
    std::vector<ChildDesc> keptList;
    for (int i = 0; i < kept; ++i) keptList.push_back(children[i]);

    ChildDesc club;
    club.name = QString("➕ [%1 Files/Folders]").arg(totalChildren - maxVisible);
    club.size = othersSum;
    club.ptr = nullptr;
    club.isVirtual = true;
    keptList.push_back(club);

    children.swap(keptList);
}

/* -------------------------
   Squarified treemap utils
   Adapted simplified squarify algorithm:
   - sizes must be non-negative
   - returns a rectangle per size in same order as sizes
   ------------------------- */

static double worstRatio(const std::vector<double> &row, double side) {
    if (row.empty() || side <= 0) return std::numeric_limits<double>::infinity();
    double sum = 0;
    for (double v : row) sum += v;
    double maxv = 0, minv = std::numeric_limits<double>::infinity();
    for (double v : row) { maxv = std::max(maxv, v); minv = std::min(minv, v); }
    double s2 = side * side;
    double r1 = (s2 * maxv) / (sum * sum);
    double r2 = (sum * sum) / (s2 * minv);
    return std::max(r1, r2);
}

std::vector<QRectF> TreeMapWidget::squarifyLayout(const std::vector<quint64> &sizes, const QRectF &rect) const
{
    std::vector<QRectF> result;
    result.reserve(sizes.size());
    // handle trivial cases
    quint64 total = 0;
    for (auto v : sizes) total += v;
    if (sizes.empty() || total == 0) {
        // all zero sizes -> return zero rects
        for (size_t i = 0; i < sizes.size(); ++i) result.push_back(QRectF(0,0,0,0));
        return result;
    }

    // We'll implement a simple squarify using a working list of remaining rectangles
    // Maintain order: we will map sizes to rectangles in the same order as input
    std::vector<double> normalized;
    normalized.reserve(sizes.size());
    for (auto v : sizes) normalized.push_back(double(v) / double(total));

    // We'll maintain an index pointer into normalized
    int idx = 0;
    QRectF avail = rect;
    std::vector<double> row;
    std::vector<int> rowIdx; // indices in normalized
    while (idx < (int)normalized.size()) {
        double val = normalized[idx];
        // try adding to row
        row.push_back(val);
        rowIdx.push_back(idx);

        // compute worst ratio for current row
        double side = std::min(avail.width(), avail.height());
        double currentWorst = worstRatio(row, side);

        // look ahead: should we commit this row or try adding more?
        double nextWorst = std::numeric_limits<double>::infinity();
        if (idx + 1 < (int)normalized.size()) {
            std::vector<double> rowPlus = row;
            rowPlus.push_back(normalized[idx + 1]);
            nextWorst = worstRatio(rowPlus, side);
        }

        if (idx + 1 == (int)normalized.size() || nextWorst > currentWorst) {
            // commit row: layout rowIdx across avail rectangle
            double rowSum = 0;
            for (double r : row) rowSum += r;

            // direction: horizontal slice if width >= height
            bool horizontal = avail.width() >= avail.height();
            if (horizontal) {
                // row height = avail.height * rowSum
                double rowHeight = avail.height() * rowSum;
                double x = avail.left();
                for (size_t k = 0; k < row.size(); ++k) {
                    double w = (row[k] / rowSum) * avail.width();
                    QRectF rrect(x, avail.top(), w, rowHeight);
                    result.push_back(rrect);
                    x += w;
                }
                // shrink avail
                avail = QRectF(avail.left(), avail.top() + rowHeight, avail.width(), avail.height() - rowHeight);
            } else {
                // vertical slice: row width = avail.width * rowSum
                double rowWidth = avail.width() * rowSum;
                double y = avail.top();
                for (size_t k = 0; k < row.size(); ++k) {
                    double h = (row[k] / rowSum) * avail.height();
                    QRectF rrect(avail.left(), y, rowWidth, h);
                    result.push_back(rrect);
                    y += h;
                }
                avail = QRectF(avail.left() + rowWidth, avail.top(), avail.width() - rowWidth, avail.height());
            }

            // clear row
            row.clear();
            rowIdx.clear();
        }
        ++idx;
    }

    // result now has rectangles but in the order of assignment (which follows original order)
    // However because we committed rows in sequence, result size should equal input size
    // If slightly mismatched, pad with zero rects
    if ((int)result.size() != (int)sizes.size()) {
        // attempt to adjust: if fewer, append small rects using remaining avail area
        while ((int)result.size() < (int)sizes.size()) result.push_back(QRectF(0,0,0,0));
    }
    return result;
}

/* -------------------------
   Drawing
   ------------------------- */

void TreeMapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().window());

    if (!m_tree || m_tree->topLevelItemCount() == 0) return;
    QTreeWidgetItem *rootItem = m_tree->topLevelItem(0);
    if (!rootItem) return;

    quint64 absoluteRootSize = itemRawSize(rootItem);
    if (absoluteRootSize == 0) {
        painter.setPen(Qt::black);
        painter.drawText(rect().adjusted(6,6,-6,-6), Qt::AlignLeft | Qt::AlignTop, "No size data");
        return;
    }

    // Reserve legend space at bottom
    const qreal legendHeight = 44.0;
    QRectF mainRect = QRectF(0, 0, width(), height() - legendHeight).adjusted(m_margin, m_margin, -m_margin, -m_margin);

    drawItem(painter, mainRect, rootItem, 0, absoluteRootSize);

    QRectF legendRect(0, height() - legendHeight - 4, width(), legendHeight);
    drawLegend(painter, legendRect);
}

void TreeMapWidget::drawVirtualItem(QPainter &painter, const QRectF &rect, const ChildDesc &virtualChild,
                                    QTreeWidgetItem *parentItem)
{
    // draw box
    QColor color = m_depthColors[ std::min(4, 1) ]; // color choice will be handled in caller (depth+1), simplified here
    painter.setBrush(color);
    QPen pen(m_borderColor);
    pen.setWidthF(1.0);
    painter.setPen(pen);
    QRectF outer = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRect(outer);

    // label only if fits
    QFontMetrics fm(painter.font());
    QString percent = "0.0%";
    if (parentItem) {
        quint64 psize = itemRawSize(parentItem);
        if (psize > 0) percent = QString::number(100.0 * double(virtualChild.size) / double(psize), 'f', 1) + "%";
    }
    QString label = QString("%1 (%2)").arg(virtualChild.name).arg(percent);

    int availW = int(outer.width() - 8);
    if (availW > 10 && fm.horizontalAdvance(label) <= availW) {
        painter.setPen(Qt::black);
        painter.drawText(outer.adjusted(4,2,-4,-2), Qt::AlignLeft | Qt::AlignTop, label);
    }
}

void TreeMapWidget::drawItem(QPainter &painter, const QRectF &rect, QTreeWidgetItem *item, int depth,
                             quint64 absoluteRootSize)
{
    if (!item) return;

    // Skip node entirely if it doesn't pass absolute-root threshold (except root itself)
    if (item->parent() != nullptr && !passesRootVisibility(item, absoluteRootSize)) return;

    // fill
    QColor fillColor = m_depthColors[ std::min(depth, 4) ];
    painter.setBrush(fillColor);
    QPen pen(m_borderColor);
    pen.setWidthF(1.0);
    painter.setPen(pen);

    QRectF outer = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRect(outer);

    // prepare font and metrics (use default app font)
    painter.setFont(QFont());
    QFontMetrics fm(painter.font());

    // compute label (prefix + name + percent of immediate parent)
    QString prefix = (item->childCount() > 0) ? "📁 " : "📄 ";
    double percentOfParent = 100.0;
    QTreeWidgetItem *p = item->parent();
    if (p) {
        quint64 psize = itemRawSize(p);
        if (psize > 0) percentOfParent = 100.0 * double(itemRawSize(item)) / double(psize);
    }
    QString label = QString("%1%2 (%.1f%)").arg(prefix).arg(item->text(0)).arg(percentOfParent);

    // only draw label if it fits in top area (adaptive label suppression)
    qreal labelPadding = 6.0;
    qreal availLabelWidth = outer.width() - 2.0 * labelPadding;
    int availW = int(std::max(0.0, availLabelWidth - 4.0));
    bool labelFits = (availW > 8) && (fm.horizontalAdvance(label) <= availW);

    if (labelFits) {
        QRectF labelRect(outer.left() + labelPadding, outer.top() + 4.0, availLabelWidth, fm.height() + 4.0);
        painter.setPen(Qt::black);
        painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, label);
    }
    // If label doesn't fit, we intentionally suppress it to avoid clutter (as requested).

    // Stop recursion if depth limit or no children
    if (depth >= m_maxDepth || item->childCount() == 0) return;

    // Build filtered children (applies absolute root filter)
    std::vector<ChildDesc> children = buildFilteredChildren(item, absoluteRootSize);
    if (children.empty()) return;

    // Apply per-depth allowed children & club remainder
    int allowed = allowedChildrenForDepth(depth);
    applyDepthClubbing(children, allowed);

    // Prepare sizes vector for squarify (preserve order in children)
    std::vector<quint64> sizes;
    sizes.reserve(children.size());
    for (auto &c : children) sizes.push_back(c.size ? c.size : 1); // avoid zeros

    // compute area inside outer reserved for children (leave space at top for label if drawn)
    qreal topSpace = (labelFits ? (fm.height() + 10.0) : 6.0);
    QRectF childrenArea(outer.left() + 4.0,
                        outer.top() + topSpace,
                        outer.width() - 8.0,
                        outer.height() - topSpace - 4.0);
    if (childrenArea.width() <= 2.0 || childrenArea.height() <= 2.0) return;

    // get rectangles using squarified layout
    std::vector<QRectF> childRects = squarifyLayout(sizes, childrenArea);
    // childRects correspond to children order

    // draw children (recurse or draw virtual)
    for (size_t i = 0; i < children.size(); ++i) {
        const ChildDesc &cd = children[i];
        QRectF r = childRects[i];
        // Skip tiny rectangles (extra safety)
        if (r.width() <= 1.0 || r.height() <= 1.0) continue;

        if (cd.isVirtual) {
            // draw virtual club node (label only if fits)
            drawVirtualItem(painter, r, cd, item);
        } else {
            drawItem(painter, r, cd.ptr, depth + 1, absoluteRootSize);
        }
    }
}

void TreeMapWidget::drawLegend(QPainter &painter, const QRectF &rect)
{
    // outer border
    QPen borderPen(Qt::gray);
    borderPen.setWidthF(1.0);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect.adjusted(1,1,-1,-1));

    // draw 5 boxes with labels
    QFontMetrics fm(painter.font());
    qreal boxSize = 16.0;
    qreal spacing = 12.0;
    qreal labelWidth = 70.0;
    qreal x = rect.left() + 12.0;
    qreal y = rect.center().y() - boxSize/2.0;

    for (int lvl = 0; lvl < 5; ++lvl) {
        painter.setBrush(m_depthColors[lvl]);
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(x, y, boxSize, boxSize));

        painter.setPen(Qt::black);
        QRectF textRect(x + boxSize + 6.0, y, labelWidth, boxSize);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, QString("Level %1").arg(lvl));

        x += boxSize + 6.0 + labelWidth + spacing;
    }
}
