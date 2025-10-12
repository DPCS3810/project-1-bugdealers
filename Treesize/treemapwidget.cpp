#include "treemapwidget.h"
#include <QPainter>
#include <QDebug>
#include <algorithm>
#include <vector>
#include <QFontMetrics>
#include <numeric>
#include <limits>
#include <QFont>
#include <QToolTip>
#include <QMouseEvent>
#include <QStringList>

// ---------------------- Constructor / Setup ----------------------

TreeMapWidget::TreeMapWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(240, 180);
    // enable hover detection
    setMouseTracking(true);
}

// ---------------------- Public API ----------------------

void TreeMapWidget::setSourceTree(QTreeWidget *tree, int maxDepth)
{
    m_tree = tree;
    m_maxDepth = maxDepth;
    // Clear any current root (we will default to top-level)
    m_currentRoot = nullptr;
    update();
}

void TreeMapWidget::clearSource()
{
    m_tree = nullptr;
    m_currentRoot = nullptr;
    update();
}

void TreeMapWidget::setCurrentRoot(QTreeWidgetItem *item)
{
    m_currentRoot = item;
    update();
    emit rootChanged(m_currentRoot);
}

void TreeMapWidget::clearCurrentRoot()
{
    m_currentRoot = nullptr;
    update();
    emit rootChanged(nullptr);
}

// ---------------------- Helpers ----------------------

quint64 TreeMapWidget::itemRawSize(QTreeWidgetItem *item) const
{
    if (!item) return 0;
    QVariant v = item->data(1, Qt::UserRole);
    if (v.isValid()) {
        bool ok = false;
        // if stored as number string in userrole
        quint64 val = v.toString().toULongLong(&ok);
        if (ok) return val;
    }
    // fallback parse from visible text column 1
    QString text = item->text(1).trimmed();
    QString numberPart = text.split(' ').first().replace(",", "");
    bool ok = false;
    quint64 fallback = numberPart.toULongLong(&ok);
    if (ok) return fallback;
    return 0;
}

QString TreeMapWidget::humanReadableSize(quint64 bytes) const
{
    double val = double(bytes);
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    while (val >= 1024.0 && unit < 4) { val /= 1024.0; ++unit; }
    return QString::number(val, 'f', (unit==0?0:2)) + " " + units[unit];
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

int TreeMapWidget::getItemDepth(QTreeWidgetItem *item) const
{
    int d = 0;
    QTreeWidgetItem *p = item;
    while (p && p->parent()) {
        ++d;
        p = p->parent();
    }
    return d;
}

// ---------------------- Children building & clubbing ----------------------

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
        // club all into one node
        int count = children.size();
        quint64 total = 0;
        for (auto &c : children) total += c.size;
        children.clear();
        if (total > 0) {
            ChildDesc club;
            club.name = QString("➕ [%1 Files/Folders]").arg(count);
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

// ---------------------- Squarified treemap ----------------------

static double worstRatio(const std::vector<double> &row, double side) {
    if (row.empty() || side <= 0) return std::numeric_limits<double>::infinity();
    double sum = 0;
    for (double v : row) sum += v;
    double maxv = 0, minv = std::numeric_limits<double>::infinity();
    for (double v : row) { maxv = std::max(maxv, v); minv = std::min(minv, v); }
    if (minv <= 0 || sum <= 0) return std::numeric_limits<double>::infinity();
    double s2 = side * side;
    double r1 = (s2 * maxv) / (sum * sum);
    double r2 = (sum * sum) / (s2 * minv);
    return std::max(r1, r2);
}

std::vector<QRectF> TreeMapWidget::squarifyLayout(const std::vector<quint64> &sizes, const QRectF &rect) const
{
    std::vector<QRectF> result;
    // handle trivial cases
    quint64 total = 0;
    for (auto v : sizes) total += v;
    if (sizes.empty() || total == 0) {
        // all zero sizes -> return zero rects
        for (size_t i = 0; i < sizes.size(); ++i) result.push_back(QRectF(0,0,0,0));
        return result;
    }

    // normalized fractions
    std::vector<double> normalized;
    normalized.reserve(sizes.size());
    for (auto v : sizes) normalized.push_back(double(v) / double(total));

    // initialize result with placeholder rects of correct size
    result.assign(normalized.size(), QRectF(0,0,0,0));

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
                    // place into result at original index
                    result[rowIdx[k]] = rrect;
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
                    result[rowIdx[k]] = rrect;
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

    return result;
}

// ---------------------- Drawing ----------------------

void TreeMapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.fillRect(rect(), palette().window());

    // clear rectangle->item map before painting
    m_rectMap.clear();
    m_hoveredItem = nullptr; // will be set on mouse move

    if (!m_tree || m_tree->topLevelItemCount() == 0) return;

    QTreeWidgetItem *rootItem = nullptr;
    if (m_currentRoot) rootItem = m_currentRoot;
    else rootItem = m_tree->topLevelItem(0);

    if (!rootItem) return;

    quint64 absoluteRootSize = itemRawSize(rootItem);
    if (absoluteRootSize == 0) {
        painter.setPen(Qt::black);
        painter.drawText(rect().adjusted(6,6,-6,-6), Qt::AlignLeft | Qt::AlignTop, "No size data");
        return;
    }

    // Reserve legend space at bottom
    const qreal legendHeight = 36.0;
    QRectF mainRect = QRectF(0, 0, width(), height() - legendHeight).adjusted(m_margin, m_margin, -m_margin, -m_margin);

    drawItem(painter, mainRect, rootItem, 0, absoluteRootSize);

    QRectF legendRect(0, height() - legendHeight, width(), legendHeight);
    drawLegend(painter, legendRect);
}

void TreeMapWidget::drawVirtualItem(QPainter &painter, const QRectF &rect, const ChildDesc &virtualChild,
                                    QTreeWidgetItem *parentItem)
{
    // choose color based on parent depth (if available), else level 1
    int depth = 1;
    if (parentItem) depth = getItemDepth(parentItem) + 1;
    depth = std::min(depth, 4);

    QColor color = m_depthColors[ depth ];
    painter.setBrush(color);
    QPen pen(m_borderColor);
    pen.setWidthF(1.0);
    painter.setPen(pen);
    QRectF outer = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRect(outer);

    // record mapping for hit-detection: virtual nodes map to nullptr so clicks don't change the root
    m_rectMap.append(qMakePair(outer, (QTreeWidgetItem*)nullptr));

    // label: name + percentage of parent
    QFontMetrics fm(painter.font());
    QString percent = "0.0%";
    if (parentItem) {
        quint64 psize = itemRawSize(parentItem);
        if (psize > 0) percent = QString::number(100.0 * double(virtualChild.size) / double(psize), 'f', 1) + "%";
    }
    QString label = QString("%1 (%2)").arg(virtualChild.name).arg(percent);

    int availW = int(outer.width() - 8);
    if (availW > 10) {
        // if full label fits, draw; else draw elided if at least half fits
        int fullW = fm.horizontalAdvance(label);
        QString displayLabel;
        if (fullW <= availW) {
            displayLabel = label;
        } else {
            // require at least half of the full width to show a truncated label
            if (availW >= fullW / 2) {
                displayLabel = fm.elidedText(label, Qt::ElideRight, availW);
            } else {
                displayLabel.clear();
            }
        }

        if (!displayLabel.isEmpty()) {
            painter.setPen(Qt::black);
            painter.drawText(outer.adjusted(4,2,-4,-2), Qt::AlignLeft | Qt::AlignTop, displayLabel);
        }
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

    // record mapping for hit-detection - clickable only if ptr non-null (we store item pointer)
    m_rectMap.append(qMakePair(outer, item));

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
    // fixed percent formatting
    QString label = QString("%1%2 (%3%)")
                        .arg(prefix)
                        .arg(item->text(0))
                        .arg(percentOfParent, 0, 'f', 1);

    // only draw label if it fits in top area (adaptive label suppression with elide)
    qreal labelPadding = 6.0;
    qreal availLabelWidth = outer.width() - 2.0 * labelPadding;
    int availW = int(std::max(0.0, availLabelWidth - 4.0));
    int labelWidth = fm.horizontalAdvance(label);

    if (availW > 8) {
        QString displayLabel = label;
        if (labelWidth > availW) {
            // show truncated text with ellipsis if at least half fits
            if (availW >= labelWidth / 2) {
                displayLabel = fm.elidedText(label, Qt::ElideRight, availW);
            } else {
                displayLabel.clear(); // too small even for ellipsis
            }
        }

        if (!displayLabel.isEmpty()) {
            QRectF labelRect(outer.left() + labelPadding, outer.top() + 4.0, availLabelWidth, fm.height() + 4.0);
            painter.setPen(Qt::black);
            painter.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, displayLabel);
        }
    }

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
    qreal topSpace = (/* if some label displayed? */ (fm.horizontalAdvance(label) <= availW) ? (fm.height() + 10.0) : 6.0);
    // Note: we used a simpler heuristic: if the full label fits we reserve the larger top space,
    // otherwise reserve a small top margin. This keeps children area sensible when label is elided.
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
    // outer border covers full width (use a thin border)
    painter.setPen(QPen(Qt::gray, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));

    // Divide into 5 equal inner boxes
    int levels = 5;
    qreal sectionWidth = rect.width() / levels;
    qreal y = rect.top();
    qreal h = rect.height();

    QFontMetrics fm(painter.font());

    for (int i = 0; i < levels; ++i) {
        QRectF section(rect.left() + i * sectionWidth, y, sectionWidth, h);

        painter.setBrush(m_depthColors[i]);
        painter.setPen(Qt::NoPen);
        painter.drawRect(section);

        painter.setPen(Qt::black);
        QString text = QString("Level %1").arg(i);
        painter.drawText(section, Qt::AlignCenter, text);
    }
}

// ---------------------- Interaction: hit-detection & tooltips ----------------------

void TreeMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_tree) return;

    QPointF pos = event->pos();
    // iterate from end so we find top-most rectangles drawn last (children over parents)
    for (int i = m_rectMap.size() - 1; i >= 0; --i) {
        const QRectF &r = m_rectMap[i].first;
        QTreeWidgetItem *it = m_rectMap[i].second;
        if (r.contains(pos)) {
            // only respond if it's a real item (not virtual club node)
            if (it) {
                // change current root (no rescanning)
                m_currentRoot = it;
                update();
                emit rootChanged(m_currentRoot);
            }
            return;
        }
    }
    // if click outside any rectangle, do nothing
    QWidget::mousePressEvent(event);
}

void TreeMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_tree) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    QPointF pos = event->pos();
    QTreeWidgetItem *found = nullptr;
    // iterate reverse so top-most rectangles get priority
    for (int i = m_rectMap.size() - 1; i >= 0; --i) {
        const QRectF &r = m_rectMap[i].first;
        QTreeWidgetItem *it = m_rectMap[i].second;
        if (r.contains(pos)) {
            found = it; // may be nullptr for virtual club nodes
            break;
        }
    }

    // if hovered changed, update tooltip
    if (found != m_hoveredItem) {
        m_hoveredItem = found;
        if (m_hoveredItem) {
            // build tooltip text from available QTreeWidgetItem data
            QString name = m_hoveredItem->text(0);
            quint64 size = itemRawSize(m_hoveredItem);
            QString sizeStr = humanReadableSize(size);

            // attempt percentage of parent
            QString pctStr = "100%";
            QTreeWidgetItem *p = m_hoveredItem->parent();
            if (p) {
                quint64 psize = itemRawSize(p);
                if (psize > 0) pctStr = QString::number(100.0 * double(size) / double(psize), 'f', 1) + "%";
            }

            // try to pick last-modified info from column 2 if present, else from userrole
            QString modified = "Unknown";
            QVariant mv = m_hoveredItem->data(2, Qt::UserRole);
            if (mv.isValid()) modified = mv.toString();
            else if (!m_hoveredItem->text(2).isEmpty()) modified = m_hoveredItem->text(2);

            QString tooltip = QString("%1\nSize: %2\n%3 of parent\nModified: %4")
                                  .arg(name)
                                  .arg(sizeStr)
                                  .arg(pctStr)
                                  .arg(modified);
            QToolTip::showText(event->globalPos(), tooltip, this);
        } else {
            // hovered over empty or a virtual node (nullptr) -> hide tooltip
            QToolTip::hideText();
        }
    }

    QWidget::mouseMoveEvent(event);
}
