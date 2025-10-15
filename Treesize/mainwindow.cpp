#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "treemapwidget.h"
#include <QActionGroup>
#include <QProcess>
#include <QPainter>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QDir>
#include <QFileInfo>
#include <QTreeWidgetItem>
#include <QDebug>
#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QFont>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextDocument>
#include <QPrinter>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTextStream>
#include <QPrintDialog>
#include <QTextDocument>
#include <QClipboard>
#include <QApplication>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QPieSlice>
#include <QVBoxLayout>
#include <QDialog>
#include <functional>
#include <QCryptographicHash>
#include <QFile>
#include <QTextBrowser>
#include <algorithm>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialize TreeMap widget in right panel
    treeMapWidget = new TreeMapWidget(ui->widget_3);
    treeMapWidget->setVisible(false);

    QVBoxLayout *rightLayout = new QVBoxLayout(ui->widget_3);
    rightLayout->setContentsMargins(0,0,0,0);
    rightLayout->addWidget(treeMapWidget);

    // Graphical view toggle
    connect(ui->pushButton_6, &QPushButton::clicked, [this]() {
        graphicalViewEnabled = !graphicalViewEnabled;
        treeMapWidget->setVisible(graphicalViewEnabled);
        if (graphicalViewEnabled) {
            treeMapWidget->setSourceTree(ui->treeWidget, 5);
        } else {
            treeMapWidget->clearSource();
        }
    });

    // Button connections
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(ui->pushButton_7, &QPushButton::clicked, this, &MainWindow::onTextViewSettingsClicked);
    connect(ui->pushButton_4, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    connect(ui->pushButton_3, &QPushButton::clicked, this, &MainWindow::onFilterClicked);
    connect(ui->pushButton_9, &QPushButton::clicked, this, &MainWindow::onReScanClicked);
    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::onCancelScanClicked);
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onExportClicked);
    connect(ui->pushButton_10, &QPushButton::clicked, this, &MainWindow::onFreeSpaceClicked);
    // Connect TreeView clicks to graphical view drill-down
    connect(ui->treeWidget, &QTreeWidget::itemClicked,
            this, &MainWindow::onTreeItemClicked);


    // --- Graphical Settings button setup (pushButton_8) ---
    QMenu *graphicsMenu = new QMenu(this);

    // Submenu for Max Depth
    QMenu *maxDepthMenu = new QMenu("Max Depth", graphicsMenu);

    // Create depth options 1–5
    for (int d = 1; d <= 5; ++d) {
        QAction *depthAction = maxDepthMenu->addAction(QString("Depth %1").arg(d));
        depthAction->setData(d);
        connect(depthAction, &QAction::triggered, this, [this, depthAction]() {
            int chosenDepth = depthAction->data().toInt();
            onMaxDepthSelected(chosenDepth);
        });
    }

    // Add the submenu to the main button menu
    graphicsMenu->addMenu(maxDepthMenu);

    // Attach menu to the button
    ui->pushButton_8->setMenu(graphicsMenu);

    // Optional: Show current depth in button text initially
    ui->pushButton_8->setText("Graphical Settings");


    // Initialize tree widget
    ui->labelCurrentDir->setText("Current Directory: —");
    ui->treeWidget->setColumnCount(5);
    QStringList headers = {"Name", "Size", "% of Parent", "Last Modified", "File Count"};
    ui->treeWidget->setHeaderLabels(headers);
    ui->treeWidget->setSortingEnabled(true);
    ui->treeWidget->sortByColumn(0, Qt::AscendingOrder);
    ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::onTreeItemCustomContextMenu);

    // Initialize progress bar
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("0%");
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::onMaxDepthSelected(int depth)
{
    if (treeMapWidget) {
        treeMapWidget->setMaxDepth(depth);
        ui->pushButton_8->setText(QString("Graphical Settings (Depth %1)").arg(depth));
    }
}

// Recursive DFS scan
void MainWindow::scanDirectory(const QString &path, FileNode &node)
{
    if (cancelScan) return; // stop immediately if cancelled
    QDir dir(path);
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

    for (QFileInfo entry : entries) {
        if (cancelScan) return; // stop mid-loop if cancelled
        FileNode child;
        child.name = entry.fileName();
        child.path = entry.filePath();
        child.isFolder = entry.isDir();
        child.size = entry.isFile() ? entry.size() : 0;
        child.lastModified = entry.lastModified();

        if (child.isFolder) {
            scanDirectory(child.path, child);
            node.size += child.size;
            node.fileCount += child.fileCount;
        } else {
            node.size += child.size;
            node.fileCount += 1;
        }

        node.children.append(child);

        // ---- update progress ----
        scannedItems++;
        int percent = (totalItems > 0) ? int((scannedItems * 100) / totalItems) : 0;
        ui->progressBar->setValue(percent);

        QString elapsed = QString::number(timer.elapsed() / 1000.0, 'f', 1) + "s";
        ui->progressBar->setFormat(QString("%1% (%2 elapsed)").arg(percent).arg(elapsed));

        QApplication::processEvents(); // keep UI responsive
    }
}


void MainWindow::onCancelScanClicked()
{
    if (!currentDirPath.isEmpty() && !cancelScan) {
        cancelScan = true;  // signal scanDirectory to stop

        // Clear the tree view
        ui->treeWidget->clear();

        // Reset progress bar
        ui->progressBar->setValue(0);
        ui->progressBar->setFormat("0%");

        // Reset any scan counters
        scannedItems = 0;
        totalItems = 0;

        QMessageBox::information(this, "Scan Cancelled", "The scan has been cancelled.");
    }
}



void MainWindow::onScanClicked()
{

    QString dirPath = QFileDialog::getExistingDirectory(this, "Select Folder to Scan", QDir::homePath());
    if (dirPath.isEmpty()) return;

    cancelScan = false;      // allow scan to proceed
    scannedItems = 0;        // reset progress counter
    timer.start();           // start elapsed timer
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("0%");

    currentDirPath = dirPath; // store for re-scan
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("0%");

    ui->treeWidget->clear();
    ui->labelCurrentDir->setText("Current Directory: " + dirPath);

    // Reset progress
    totalItems = 0;
    scannedItems = 0;
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("0%");

    // Count total items
    countTotalItems(dirPath);

    // Start elapsed timer
    timer.start();

    /* Create root node
    FileNode rootNode;
    rootNode.name = dirPath;
    rootNode.path = dirPath;
    rootNode.isFolder = true;
    */
    // **CHANGE THIS: Store in member variable**

    rootNode = FileNode();  // Clear previous data
    rootNode.name = dirPath;
    rootNode.path = dirPath;
    rootNode.isFolder = true;

    // Scan directory
    scanDirectory(dirPath, rootNode);

    //Check if scan was cancelled before populating the tree**
    if (cancelScan) {
        // Do not show any results
        ui->treeWidget->clear();
        ui->progressBar->setValue(0);
        ui->progressBar->setFormat("0%");
        return;
    }

    // Populate tree
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->treeWidget);
    rootItem->setText(0, rootNode.name);
    rootItem->setText(1, QString::number(rootNode.size));
    rootItem->setData(1, Qt::UserRole, QVariant::fromValue((qulonglong)rootNode.size));
    rootItem->setText(2, "100%");
    rootItem->setText(3, rootNode.lastModified.toString("yyyy-MM-dd hh:mm"));
    rootItem->setText(4, QString::number(rootNode.fileCount));

    rootItem->setData(0, Qt::UserRole, rootNode.path);
    populateTree(rootNode, rootItem);
    ui->treeWidget->addTopLevelItem(rootItem);
    ui->treeWidget->topLevelItem(0)->setExpanded(true);
    updateTreeDisplay();

    // Set progress to 100% at the end
    ui->progressBar->setValue(100);
    ui->progressBar->setFormat("100% (Done)");

    // Update graphical view if it's visible (reuse tree widget; no rescan)
    if (graphicalViewEnabled)
        treeMapWidget->setSourceTree(ui->treeWidget, 5);
}


void MainWindow::onTextViewSettingsClicked()
{
    // Create main menu
    QMenu menu(this);

    // Add submenus
    QMenu *sizeMenu = menu.addMenu("Size View (KB, MB, GB)");
    QAction *kbAct = sizeMenu->addAction("KB");
    QAction *mbAct = sizeMenu->addAction("MB");
    QAction *gbAct = sizeMenu->addAction("GB");
    sizeMenu->addSeparator();
    QAction *bytesAct = sizeMenu->addAction("Bytes");

    QAction *fontSizeAct = menu.addAction("Font Size");

    // Connect actions
    connect(kbAct, &QAction::triggered, [this]() { currentUnit = KB; updateTreeDisplay(); });
    connect(mbAct, &QAction::triggered, [this]() { currentUnit = MB; updateTreeDisplay(); });
    connect(gbAct, &QAction::triggered, [this]() { currentUnit = GB; updateTreeDisplay(); });
    connect(bytesAct, &QAction::triggered, [this]() { currentUnit = BYTES; updateTreeDisplay(); });
    connect(fontSizeAct, &QAction::triggered, this, &MainWindow::onFontSizeSelected);

    // Show menu under the button
    menu.exec(ui->pushButton_7->mapToGlobal(QPoint(0, ui->pushButton_7->height())));
}

void MainWindow::onSearchClicked()
{
    if (!searchBox) {
        // Create the search box once
        searchBox = new QLineEdit(this);
        searchBox->setPlaceholderText("Search files or folders...");
        searchBox->setClearButtonEnabled(true);
        searchBox->setFixedWidth(250);

        // Position it under the toolbar button
        QPoint globalPos = ui->pushButton_4->mapToGlobal(QPoint(0, ui->pushButton_4->height()));
        searchBox->move(mapFromGlobal(globalPos) + QPoint(0, 5));

        connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    }

    // Toggle visibility
    if (searchBox->isVisible()) {
        searchBox->hide();
        searchBox->clear();
        // Restore tree visibility
        onSearchTextChanged("");
    } else {
        searchBox->show();
        searchBox->setFocus();
    }
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    QString query = text.trimmed();

    // If search is empty, clear highlights and restore view
    if (query.isEmpty()) {
        for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
            clearHighlight(ui->treeWidget->topLevelItem(i));
            ui->treeWidget->topLevelItem(i)->setHidden(false);
        }
        ui->treeWidget->collapseAll();
        return;
    }

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        filterAndHighlightTree(ui->treeWidget->topLevelItem(i), query);
    }
}

bool MainWindow::filterAndHighlightTree(QTreeWidgetItem *item, const QString &query)
{
    bool match = item->text(0).contains(query, Qt::CaseInsensitive);

    // Highlight if match found
    if (match) {
        item->setBackground(0, QBrush(QColor(255, 255, 150))); // light yellow highlight
        item->setForeground(0, QBrush(Qt::black));
    } else {
        item->setBackground(0, QBrush(Qt::NoBrush));
        item->setForeground(0, QBrush(Qt::black));
    }

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        bool childVisible = filterAndHighlightTree(item->child(i), query);
        item->child(i)->setHidden(!childVisible);
        if (childVisible)
            childMatch = true;
    }

    bool visible = match || childMatch;
    item->setHidden(!visible);

    // Expand items that have visible children or a match
    if (visible && (match || childMatch))
        ui->treeWidget->expandItem(item);
    else
        ui->treeWidget->collapseItem(item);

    return visible;
}

void MainWindow::clearHighlight(QTreeWidgetItem *item)
{
    item->setBackground(0, QBrush(Qt::NoBrush));
    item->setForeground(0, QBrush(Qt::black));

    for (int i = 0; i < item->childCount(); ++i)
        clearHighlight(item->child(i));
}


bool MainWindow::filterTree(QTreeWidgetItem *item, const QString &query)
{
    bool match = query.isEmpty() || item->text(0).contains(query, Qt::CaseInsensitive);

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        bool childVisible = filterTree(item->child(i), query);
        item->child(i)->setHidden(!childVisible);
        if (childVisible) childMatch = true;
    }

    bool visible = match || childMatch;
    item->setHidden(!visible);
    return visible;
}

void MainWindow::updateTreeDisplay()
{
    // Helper to format bytes according to currentUnit
    auto formatSize = [this](quint64 bytes) -> QString {
        double val = (double)bytes;
        QString suffix = " bytes";
        switch (currentUnit) {
        case KB: val = val / 1024.0; suffix = " KB"; break;
        case MB: val = val / (1024.0 * 1024.0); suffix = " MB"; break;
        case GB: val = val / (1024.0 * 1024.0 * 1024.0); suffix = " GB"; break;
        default: break;
        }
        return QString::number(val, 'f', 2) + suffix;
    };

    std::function<void(QTreeWidgetItem*)> updateItem = [&](QTreeWidgetItem *item) {
        if (!item) return;

        // Prefer stored raw bytes
        quint64 sizeBytes = 0;
        QVariant v = item->data(1, Qt::UserRole);
        if (v.isValid()) {
            sizeBytes = v.toULongLong();
        } else {
            // backward-compatibility: try to parse displayed text
            bool ok = false;
            QString txt = item->text(1);
            double parsed = txt.split(" ").first().toDouble(&ok);
            if (ok) {
                if (txt.contains("KB", Qt::CaseInsensitive)) {
                    sizeBytes = (quint64)(parsed * 1024.0);
                } else if (txt.contains("MB", Qt::CaseInsensitive)) {
                    sizeBytes = (quint64)(parsed * 1024.0 * 1024.0);
                } else if (txt.contains("GB", Qt::CaseInsensitive)) {
                    sizeBytes = (quint64)(parsed * 1024.0 * 1024.0 * 1024.0);
                } else {
                    sizeBytes = (quint64)parsed;
                }
                // store corrected raw value for future
                item->setData(1, Qt::UserRole, QVariant::fromValue((qulonglong)sizeBytes));
            }
        }

        // Now set formatted text for display
        item->setText(1, formatSize(sizeBytes));

        for (int i = 0; i < item->childCount(); ++i)
            updateItem(item->child(i));
    };

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        updateItem(ui->treeWidget->topLevelItem(i));
}


void MainWindow::onFontSizeSelected()
{
    bool ok;
    int fontSize = QInputDialog::getInt(this, "Font Size", "Enter font size:",
                                        ui->treeWidget->font().pointSize(), 8, 48, 1, &ok);
    if (ok) {
        QFont font = ui->treeWidget->font();
        font.setPointSize(fontSize);
        ui->treeWidget->setFont(font);
    }
}

void MainWindow::onFilterClicked()
{
    QMenu menu(this);

    QAction *sizeAct = menu.addAction("By size");
    QAction *countAct = menu.addAction("By file count");
    QAction *formatAct = menu.addAction("By format");
    QAction *resetAct = menu.addAction("Reset filter");  // <-- new

    connect(sizeAct, &QAction::triggered, this, &MainWindow::filterBySize);
    connect(countAct, &QAction::triggered, this, &MainWindow::filterByFileCount);
    connect(formatAct, &QAction::triggered, this, &MainWindow::filterByFormat);
    connect(resetAct, &QAction::triggered, this, &MainWindow::resetFilter); // <-- new

    menu.exec(ui->pushButton_3->mapToGlobal(QPoint(0, ui->pushButton_3->height())));
}


void MainWindow::filterBySize()
{
    bool ok;
    double value = QInputDialog::getDouble(this, "Filter by size",
                                           "Enter size value:",
                                           0, 0, 1e12, 2, &ok);
    if (!ok) return;

    QStringList options = {"Less than", "Greater than"};
    bool ok2;
    QString choice = QInputDialog::getItem(this, "Choose comparison",
                                           "Filter:", options, 0, false, &ok2);
    if (!ok2) return;

    bool greater = (choice == "Greater than");

    // Convert entered value into bytes according to currentUnit
    auto toBytes = [this](double v)->quint64 {
        switch (currentUnit) {
        case BYTES: return (quint64) v;
        case KB:    return (quint64)(v * 1024.0);
        case MB:    return (quint64)(v * 1024.0 * 1024.0);
        case GB:    return (quint64)(v * 1024.0 * 1024.0 * 1024.0);
        default:    return (quint64) v;
        }
    };

    quint64 valueBytes = toBytes(value);

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        filterTreeBySize(ui->treeWidget->topLevelItem(i), valueBytes, greater);
}


bool MainWindow::filterTreeBySize(QTreeWidgetItem *item, quint64 sizeBytes, bool greater)
{
    // Get item size from stored data (fall back to parsing text if needed)
    quint64 itemSize = 0;
    QVariant v = item->data(1, Qt::UserRole);
    if (v.isValid()) {
        itemSize = v.toULongLong();
    } else {
        // try parsing displayed text
        bool ok;
        QString txt = item->text(1);
        double parsed = txt.split(" ").first().toDouble(&ok);
        if (ok) {
            if (txt.contains("KB", Qt::CaseInsensitive)) {
                itemSize = (quint64)(parsed * 1024.0);
            } else if (txt.contains("MB", Qt::CaseInsensitive)) {
                itemSize = (quint64)(parsed * 1024.0 * 1024.0);
            } else if (txt.contains("GB", Qt::CaseInsensitive)) {
                itemSize = (quint64)(parsed * 1024.0 * 1024.0 * 1024.0);
            } else {
                itemSize = (quint64)parsed;
            }
            item->setData(1, Qt::UserRole, QVariant::fromValue((qulonglong)itemSize));
        }
    }

    bool match = greater ? (itemSize > sizeBytes) : (itemSize < sizeBytes);

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        bool childVisible = filterTreeBySize(item->child(i), sizeBytes, greater);
        item->child(i)->setHidden(!childVisible);
        if (childVisible) childMatch = true;
    }

    bool visible = match || childMatch;
    item->setHidden(!visible);
    return visible;
}


void MainWindow::filterByFileCount()
{
    bool ok;
    quint64 value = QInputDialog::getInt(this, "Filter by file count",
                                         "Enter file count:", 0, 0, 1e9, 1, &ok);
    if (!ok) return;

    QStringList options = {"Less than", "Greater than"};
    bool ok2;
    QString choice = QInputDialog::getItem(this, "Choose comparison",
                                           "Filter:", options, 0, false, &ok2);
    if (!ok2) return;

    bool greater = (choice == "Greater than");

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        filterTreeByCount(ui->treeWidget->topLevelItem(i), value, greater);
}

bool MainWindow::filterTreeByCount(QTreeWidgetItem *item, quint64 count, bool greater)
{
    quint64 fileCount = item->text(4).toULongLong();
    bool match = greater ? (fileCount > count) : (fileCount < count);

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        bool childVisible = filterTreeByCount(item->child(i), count, greater);
        item->child(i)->setHidden(!childVisible);
        if (childVisible) childMatch = true;
    }

    bool visible = match || childMatch;
    item->setHidden(!visible);
    return visible;
}

void MainWindow::filterByFormat()
{
    // Collect unique extensions from tree
    QSet<QString> extSet;
    std::function<void(QTreeWidgetItem*)> collectExt = [&](QTreeWidgetItem *item){
        if (item->childCount() == 0) {
            QString name = item->text(0);
            int dotIdx = name.lastIndexOf('.');
            if (dotIdx > 0) extSet.insert(name.mid(dotIdx).toLower());
        }
        for (int i = 0; i < item->childCount(); ++i)
            collectExt(item->child(i));
    };

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        collectExt(ui->treeWidget->topLevelItem(i));

    if (extSet.isEmpty()) return;

    QStringList extList;
    for (const QString &ext : extSet)
        extList.append(ext);

    bool ok;
    QString choice = QInputDialog::getItem(this, "Filter by format",
                                           "Choose extension:", extList, 0, false, &ok);
    if (!ok) return;

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        filterTreeByFormat(ui->treeWidget->topLevelItem(i), QStringList() << choice);
}

bool MainWindow::filterTreeByFormat(QTreeWidgetItem *item, const QStringList &extList)
{
    bool match = false;
    if (item->childCount() == 0) { // only files
        QString name = item->text(0);
        int dotIdx = name.lastIndexOf('.');
        if (dotIdx > 0) {
            QString ext = name.mid(dotIdx).toLower();
            if (extList.contains(ext)) match = true;
        }
    }

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        bool childVisible = filterTreeByFormat(item->child(i), extList);
        item->child(i)->setHidden(!childVisible);
        if (childVisible) childMatch = true;
    }

    bool visible = match || childMatch;
    item->setHidden(!visible);
    return visible;
}

void MainWindow::resetFilter()
{
    // Recursively unhide all items
    std::function<void(QTreeWidgetItem*)> unhideAll = [&](QTreeWidgetItem *item){
        if (!item) return;
        item->setHidden(false);
        for (int i = 0; i < item->childCount(); ++i)
            unhideAll(item->child(i));
    };

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        unhideAll(ui->treeWidget->topLevelItem(i));
}


void MainWindow::populateTree(const FileNode &node, QTreeWidgetItem *parentItem)
{
    for (const FileNode &child : node.children) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);

        // Name
        item->setText(0, child.name);
        // Store raw size in UserRole (source of truth)
        item->setData(1, Qt::UserRole, QVariant::fromValue((qulonglong)child.size));
        // Show initial textual size (will be refreshed by updateTreeDisplay)
        item->setText(1, QString::number(child.size));
        // Percentage of parent
        double percent = (node.size > 0) ? (100.0 * child.size / node.size) : 0.0;
        item->setText(2, QString::number(percent, 'f', 2) + "%");
        // Last modified
        item->setText(3, child.lastModified.toString("yyyy-MM-dd hh:mm"));
        // File count
        item->setText(4, QString::number(child.fileCount));

        item->setData(0, Qt::UserRole, child.path);
        if (child.isFolder)
            populateTree(child, item);
    }
}


void MainWindow::countTotalItems(const QString &path)
{
    QDir dir(path);
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

    totalItems += entries.size(); // count current folder's items

    for (QFileInfo entry : entries) {
        if (entry.isDir()) {
            countTotalItems(entry.filePath());
        }
    }
}

void MainWindow::onReScanClicked()
{
    cancelScan = false;      // allow scan to proceed
    scannedItems = 0;        // reset progress counter
    timer.start();           // start elapsed timer
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("0%");
    if (currentDirPath.isEmpty()) {
        // no folder scanned yet
        QMessageBox::information(this, "Re Scan", "No folder has been scanned yet.");
        return;
    }

    // clear tree and reset progress
    ui->treeWidget->clear();
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("0%");
    ui->labelCurrentDir->setText("Current Directory: " + currentDirPath);

    totalItems = 0;
    scannedItems = 0;
    timer.start();

    countTotalItems(currentDirPath);

    rootNode = FileNode();
    rootNode.name = currentDirPath;
    rootNode.path = currentDirPath;
    rootNode.isFolder = true;
    scanDirectory(currentDirPath, rootNode);

    //Check if scan was cancelled before populating the tree**
    if (cancelScan) {
        // Do not show any results
        ui->treeWidget->clear();
        ui->progressBar->setValue(0);
        ui->progressBar->setFormat("0%");
        return;
    }

    QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->treeWidget);
    rootItem->setText(0, rootNode.name);
    rootItem->setText(1, QString::number(rootNode.size));
    rootItem->setData(1, Qt::UserRole, QVariant::fromValue((qulonglong)rootNode.size));
    rootItem->setText(2, "100%");
    rootItem->setText(3, rootNode.lastModified.toString("yyyy-MM-dd hh:mm"));
    rootItem->setText(4, QString::number(rootNode.fileCount));

    populateTree(rootNode, rootItem);
    ui->treeWidget->addTopLevelItem(rootItem);
    ui->treeWidget->topLevelItem(0)->setExpanded(true);
    updateTreeDisplay();

    ui->progressBar->setValue(100);
    ui->progressBar->setFormat("100% (Done)");
}

void MainWindow::onExportClicked()
{
    QMenu menu(this);
    QAction *pdfAct = menu.addAction("Export as PDF");
    QAction *jsonAct = menu.addAction("Export as JSON");
    QAction *csvAct = menu.addAction("Export as CSV");

    connect(pdfAct, &QAction::triggered, this, &MainWindow::exportAsPDF);
    connect(jsonAct, &QAction::triggered, this, &MainWindow::exportAsJSON);
    connect(csvAct, &QAction::triggered, this, &MainWindow::exportAsCSV);

    menu.exec(ui->pushButton->mapToGlobal(QPoint(0, ui->pushButton->height())));
}

void MainWindow::exportAsCSV()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Save CSV", QDir::homePath(), "CSV Files (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "Name,Size,% of Parent,Last Modified,File Count\n";

    std::function<void(QTreeWidgetItem*, int)> writeItem = [&](QTreeWidgetItem *item, int level) {
        if (!item) return;
        QString indent(level * 2, ' ');  // indent for subitems
        out << indent << item->text(0) << ","
            << item->text(1) << ","
            << item->text(2) << ","
            << item->text(3) << ","
            << item->text(4) << "\n";

        for (int i = 0; i < item->childCount(); ++i)
            writeItem(item->child(i), level + 1);
    };

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        writeItem(ui->treeWidget->topLevelItem(i), 0);

    file.close();
    QMessageBox::information(this, "Export CSV", "Export completed successfully.");
}

void MainWindow::exportAsJSON()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Save JSON", QDir::homePath(), "JSON Files (*.json)");
    if (filePath.isEmpty()) return;

    std::function<QJsonObject(QTreeWidgetItem*)> treeItemToJson = [&](QTreeWidgetItem *item) -> QJsonObject {
        QJsonObject obj;
        obj["Name"] = item->text(0);
        obj["Size"] = item->text(1);
        obj["% of Parent"] = item->text(2);
        obj["Last Modified"] = item->text(3);
        obj["File Count"] = item->text(4);

        if (item->childCount() > 0) {
            QJsonArray children;
            for (int i = 0; i < item->childCount(); ++i)
                children.append(treeItemToJson(item->child(i)));
            obj["Children"] = children;
        }

        return obj;
    };

    QJsonArray rootArray;
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        rootArray.append(treeItemToJson(ui->treeWidget->topLevelItem(i)));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(rootArray).toJson(QJsonDocument::Indented));
    file.close();

    QMessageBox::information(this, "Export JSON", "Export completed successfully.");
}

void MainWindow::exportAsPDF()
{
    QString filePath = QFileDialog::getSaveFileName(this, "Save PDF", QDir::homePath(), "PDF Files (*.pdf)");
    if (filePath.isEmpty()) return;

    QString html;
    html += "<table border='1' cellspacing='0' cellpadding='3'>";
    html += "<tr><th>Name</th><th>Size</th><th>% of Parent</th><th>Last Modified</th><th>File Count</th></tr>";

    std::function<void(QTreeWidgetItem*, int)> addRows = [&](QTreeWidgetItem *item, int level) {
        if (!item) return;
        html += "<tr>";
        html += "<td>" + QString(level*2, ' ') + item->text(0) + "</td>";
        html += "<td>" + item->text(1) + "</td>";
        html += "<td>" + item->text(2) + "</td>";
        html += "<td>" + item->text(3) + "</td>";
        html += "<td>" + item->text(4) + "</td>";
        html += "</tr>";

        for (int i = 0; i < item->childCount(); ++i)
            addRows(item->child(i), level + 1);
    };

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        addRows(ui->treeWidget->topLevelItem(i), 0);

    html += "</table>";

    QTextDocument doc;
    doc.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);

    doc.print(&printer);

    QMessageBox::information(this, "Export PDF", "Export completed successfully.");
}

void MainWindow::onTreeItemCustomContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = ui->treeWidget->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction *viewPathAct = menu.addAction("View Full Path");
    QAction *pieChartAct = menu.addAction("View 1 Level Pie Chart");
    QAction *deleteAct = menu.addAction("Delete");
    QAction *renameAct = menu.addAction("Rename");

    connect(viewPathAct, &QAction::triggered, [this, item]() { showFullPath(item); });
    connect(pieChartAct, &QAction::triggered, [this, item]() { showPieChart(item); });
    connect(deleteAct, &QAction::triggered, [this, item]() { deleteItem(item); });
    connect(renameAct, &QAction::triggered, [this, item]() { renameItem(item); });

    menu.exec(ui->treeWidget->viewport()->mapToGlobal(pos));
}

void MainWindow::showFullPath(QTreeWidgetItem *item)
{
    QString fullPath = item->data(0, Qt::UserRole).toString();

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Full Path");
    msgBox.setText("Full path of the selected item:");
    msgBox.setInformativeText(fullPath);
    msgBox.setStandardButtons(QMessageBox::Ok);

    QPushButton *copyButton = msgBox.addButton("Copy Path", QMessageBox::ActionRole);

    msgBox.exec();

    if (msgBox.clickedButton() == copyButton) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(fullPath);
        QMessageBox::information(this, "Copied", "Path copied to clipboard!");
    }
}

FileNode* MainWindow::findNodeByPath(FileNode &node, const QString &path)
{
    if (node.path == path) return &node;

    for (FileNode &child : node.children) {
        FileNode *found = findNodeByPath(child, path);
        if (found) return found;
    }

    return nullptr;
}

void MainWindow::showPieChart(QTreeWidgetItem *item)
{
    QString itemPath = item->data(0, Qt::UserRole).toString();

    // Find the corresponding FileNode
    FileNode *node = findNodeByPath(rootNode, itemPath);

    if (!node || !node->isFolder) {
        QMessageBox::warning(this, "Pie Chart", "Please select a folder to view its pie chart.");
        return;
    }

    if (node->children.isEmpty()) {
        QMessageBox::information(this, "Pie Chart", "This folder has no children to display.");
        return;
    }

    // Create pie series (without QtCharts:: prefix)
    QPieSeries *series = new QPieSeries();

    for (const FileNode &child : node->children) {
        double percentage = (node->size > 0) ? (100.0 * child.size / node->size) : 0.0;
        QPieSlice *slice = series->append(child.name, child.size);
        slice->setLabel(QString("%1 (%2%)").arg(child.name).arg(percentage, 0, 'f', 2));
    }

    // Create chart (without QtCharts:: prefix)
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Direct Children of: " + node->name);
    chart->legend()->setAlignment(Qt::AlignRight);

    // Create chart view (without QtCharts:: prefix)
    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // Create dialog to show chart
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Pie Chart - " + node->name);
    dialog->resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->addWidget(chartView);
    dialog->setLayout(layout);

    dialog->exec();
}

bool MainWindow::moveToRecycleBin(const QString &path)
{
#ifdef Q_OS_WIN
    // Convert QString to wide string for Windows API
    std::wstring wPath = path.toStdWString();

    // Double null-terminated string required by SHFileOperation
    std::vector<wchar_t> doubleNullTerminated(wPath.begin(), wPath.end());
    doubleNullTerminated.push_back(L'\0');
    doubleNullTerminated.push_back(L'\0');

    SHFILEOPSTRUCTW fileOp = {};
    fileOp.hwnd = nullptr;
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = doubleNullTerminated.data();
    fileOp.pTo = nullptr;
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NO_UI;  // FOF_ALLOWUNDO sends to recycle bin
    fileOp.fAnyOperationsAborted = FALSE;
    fileOp.hNameMappings = nullptr;
    fileOp.lpszProgressTitle = nullptr;

    int result = SHFileOperationW(&fileOp);

    return (result == 0);

#elif defined(Q_OS_MAC)
    // macOS: Move to Trash using NSFileManager
    QProcess process;
    process.start("osascript", QStringList()
                                   << "-e"
                                   << QString("tell application \"Finder\" to delete POSIX file \"%1\"").arg(path));
    process.waitForFinished();
    return (process.exitCode() == 0);

#elif defined(Q_OS_LINUX)
    // Linux: Move to Trash using gio trash command
    QProcess process;
    process.start("gio", QStringList() << "trash" << path);
    process.waitForFinished();

    if (process.exitCode() != 0) {
        // Fallback: try trash-cli
        process.start("trash-put", QStringList() << path);
        process.waitForFinished();
    }

    return (process.exitCode() == 0);

#else
    // Unsupported platform - return false
    return false;
#endif
}

void MainWindow::deleteItem(QTreeWidgetItem *item)
{
    QString itemPath = item->data(0, Qt::UserRole).toString();
    QString itemName = item->text(0);

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete Confirmation",
                                  "Are you sure you want to delete:\n" + itemName +
                                      "\n\nThe item will be moved to the Recycle Bin.",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // Show progress bar
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("Deleting...");
    QApplication::processEvents();

    // Use the new moveToRecycleBin function
    bool success = moveToRecycleBin(itemPath);

    if (!success) {
        ui->progressBar->setValue(0);
        ui->progressBar->setFormat("0%");
        QMessageBox::warning(this, "File in Use Warning",
                             "This file cannot be deleted. It may be in use by another program or you may not have permission.");
        return;
    }

    ui->progressBar->setValue(100);
    ui->progressBar->setFormat("Delete Complete");
    QApplication::processEvents();

    QMessageBox::information(this, "Deletion Complete",
                             "Item moved to Recycle Bin successfully. Rescanning directory...");

    // Automatic rescan
    onReScanClicked();
}

/*void MainWindow::deleteItem(QTreeWidgetItem *item)
{
    QString itemPath = item->data(0, Qt::UserRole).toString();
    QString itemName = item->text(0);

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete Confirmation",
                                  "Are you sure you want to delete:\n" + itemName + "?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    // Show progress bar
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("Deleting...");
    QApplication::processEvents();

    QFileInfo fileInfo(itemPath);
    bool success = false;

    if (fileInfo.isDir()) {
        QDir dir(itemPath);
        success = dir.removeRecursively();
    } else {
        QFile file(itemPath);
        success = file.remove();
    }

    if (!success) {
        ui->progressBar->setValue(0);
        ui->progressBar->setFormat("0%");
        QMessageBox::warning(this, "File in Use Warning",
                             "This file cannot be deleted. It may be in use by another program.");
        return;
    }

    ui->progressBar->setValue(100);
    ui->progressBar->setFormat("Delete Complete");
    QApplication::processEvents();

    QMessageBox::information(this, "Deletion Complete",
                             "Item deleted successfully. Rescanning directory...");

    // Automatic rescan
    onReScanClicked();
}
*/

void MainWindow::renameItem(QTreeWidgetItem *item)
{
    QString oldPath = item->data(0, Qt::UserRole).toString();
    QString oldName = item->text(0);

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename",
                                            "Enter new name:",
                                            QLineEdit::Normal,
                                            oldName, &ok);

    if (!ok || newName.isEmpty() || newName == oldName) return;

    QFileInfo fileInfo(oldPath);
    QString newPath = fileInfo.absolutePath() + "/" + newName;

    // Check if file with same name exists
    if (QFile::exists(newPath)) {
        QMessageBox::warning(this, "Rename Error",
                             "File by the same name already exists in this directory.");
        return;
    }

    // Perform rename
    bool success = QFile::rename(oldPath, newPath);

    if (!success) {
        QMessageBox::warning(this, "Rename Error",
                             "Failed to rename the item. It may be in use or you may not have permission.");
        return;
    }

    QMessageBox::information(this, "Rename Complete",
                             "Item renamed successfully. Rescanning directory...");

    // Automatic rescan
    onReScanClicked();
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    if (!item)
        return;

    // --- Drill down into clicked folder/file in graphical view ---
    graphicalViewEnabled = true;  // activate graphical mode
    treeMapWidget->setVisible(true);

    treeMapWidget->setCurrentRoot(item);

    treeMapWidget->update();

    QString path = item->text(0);
    if (item->parent()) {
        // build full path if available via stored FileNode
        QVariant nodeVariant = item->data(0, Qt::UserRole);
        if (nodeVariant.isValid()) {
            FileNode* node = nodeVariant.value<FileNode*>();
            if (node)
                path = node->path;
        }
    }

    ui->labelCurrentDir->setText(QString("Current Directory: %1").arg(path));
}

// ================== FREE SPACE ANALYSIS FUNCTIONS ==================

void MainWindow::onFreeSpaceClicked()
{
    if (currentDirPath.isEmpty()) {
        QMessageBox::information(this, "Free Space Analysis",
                                 "Please scan a directory first before analyzing free space options.");
        return;
    }

    QMenu menu(this);
    menu.setTitle("Smart Free Space Options");

    QAction *duplicatesSmartAct = menu.addAction("Find Potential Duplicates (Smart Hash)");
    QAction *smartDeletionAct = menu.addAction("Smart Deletion Suggestions");
    QAction *pieChartAct = menu.addAction("View Storage by File Type (Pie Chart)");

    connect(duplicatesSmartAct, &QAction::triggered, this, &MainWindow::findPotentialDuplicatesSmart);
    connect(smartDeletionAct, &QAction::triggered, this, &MainWindow::showSmartDeletionSuggestions);
    connect(pieChartAct, &QAction::triggered, this, &MainWindow::showFileTypePieChart);

    menu.exec(ui->pushButton_10->mapToGlobal(QPoint(0, ui->pushButton_10->height())));
}

QByteArray MainWindow::computePartialHash(const QString &filePath, quint64 fileSize)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    const qint64 chunkSize = 1024 * 1024;  // 1 MB
    QByteArray buffer;

    // Read first 1 MB
    buffer += file.read(chunkSize);

    // Read last 1 MB if file is large enough
    if (fileSize > chunkSize) {
        file.seek(std::max<qint64>(0, fileSize - chunkSize));
        buffer += file.read(chunkSize);
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(buffer);
    return hash.result();
}

void MainWindow::findPotentialDuplicatesSmart()
{
    QVector<SmartFileInfo> allFiles;
    collectAllFilesSmart(rootNode, allFiles);

    if (allFiles.isEmpty()) {
        QMessageBox::information(this, "Find Duplicates", "No files found in this directory.");
        return;
    }

    // Group by size first
    QMap<quint64, QVector<SmartFileInfo>> sizeGroups;
    for (auto &file : allFiles)
        sizeGroups[file.size].append(file);

    QString message;
    int groupCount = 0;

    for (auto it = sizeGroups.begin(); it != sizeGroups.end(); ++it) {
        if (it.value().size() < 2)
            continue;

        // Compute partial hashes for same-size group
        QMap<QByteArray, QVector<QString>> hashGroups;
        for (auto &file : it.value()) {
            QByteArray hash = computePartialHash(file.path, file.size);
            if (!hash.isEmpty())
                hashGroups[hash].append(file.path);
        }

        for (auto hit = hashGroups.begin(); hit != hashGroups.end(); ++hit) {
            if (hit.value().size() > 1) {
                groupCount++;
                message += QString("\nGroup %1 - %2 files likely identical:\n")
                               .arg(groupCount)
                               .arg(hit.value().size());
                for (const QString &path : hit.value())
                    message += QString("  • %1\n").arg(path);
            }
        }
    }

    if (groupCount == 0)
        message = "No duplicate files detected with partial hash check.";

    QMessageBox::information(this, "Smart Duplicate Detection", message);
}


void MainWindow::showSmartDeletionSuggestions()
{
    QVector<SmartFileInfo> allFiles;
    collectAllFilesSmart(rootNode, allFiles);

    if (allFiles.isEmpty()) {
        QMessageBox::information(this, "Smart Deletion Suggestions", "No files found in this directory.");
        return;
    }

    // --- Normalization ---
    quint64 maxSize = 1;
    for (const auto &f : allFiles)
        maxSize = std::max(maxSize, f.size);

    QDateTime now = QDateTime::currentDateTime();

    for (auto &f : allFiles) {
        // Size score (larger = higher)
        f.sizeScore = double(f.size) / maxSize;

        // Age score (older = higher)
        qint64 daysOld = f.lastModified.daysTo(now);
        f.ageScore = std::min(1.0, double(daysOld) / 365.0); // normalize up to 1 year

        // Type score (based on common redundancies)
        QString ext = f.extension.toLower();
        QStringList lowPriority = {"tmp","bak","log","cache"};
        f.typeScore = lowPriority.contains(ext) ? 1.0 : 0.0;

        // Duplicate score placeholder (if same hash detected, could be updated later)
        f.duplicateScore = 0.0;

        // Weighted total
        f.totalScore = 0.4 * f.sizeScore + 0.3 * f.ageScore + 0.2 * f.typeScore + 0.1 * f.duplicateScore;
    }

    // Sort descending by total score
    std::sort(allFiles.begin(), allFiles.end(), [](const SmartFileInfo &a, const SmartFileInfo &b) {
        return a.totalScore > b.totalScore;
    });

    int count = std::min(10, static_cast<int>(allFiles.size()));

    // --- Build visual HTML ---
    QString html = "<h3>Smart Deletion Suggestions</h3>";
    html += "<p>Files ranked by estimated redundancy potential.</p>";

    for (int i = 0; i < count; ++i) {
        const auto &f = allFiles[i];
        html += QString("<b>%1.</b> %2<br>"
                        "<i>%3</i><br>"
                        "<b>Score:</b> %4<br>"
                        "<ul>"
                        "<li>Size factor: %5</li>"
                        "<li>Age factor: %6</li>"
                        "<li>Type factor: %7</li>"
                        "</ul><hr>")
                    .arg(i + 1)
                    .arg(f.name)
                    .arg(f.path)
                    .arg(QString::number(f.totalScore, 'f', 2))
                    .arg(QString::number(f.sizeScore * 100, 'f', 1) + "%")
                    .arg(QString::number(f.ageScore * 100, 'f', 1) + "%")
                    .arg(QString::number(f.typeScore * 100, 'f', 1) + "%");
    }

    // --- Display in a styled dialog ---
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Smart Deletion Suggestions");
    dialog->resize(700, 600);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QTextBrowser *browser = new QTextBrowser(dialog);
    browser->setHtml(html);
    layout->addWidget(browser);
    dialog->setLayout(layout);
    dialog->exec();
}

void MainWindow::showFileTypePieChart()
{
    QVector<SmartFileInfo> allFiles;
    collectAllFilesSmart(rootNode, allFiles);

    if (allFiles.isEmpty()) {
        QMessageBox::information(this, "View by File Type", "No files found in the scanned directory.");
        return;
    }

    QMap<QString, QStringList> categories;
    categories["Images"] = {"jpg","jpeg","png","gif","bmp","svg","ico","webp"};
    categories["Videos"] = {"mp4","avi","mkv","mov","wmv","flv","webm","m4v"};
    categories["Documents"] = {"pdf","doc","docx","txt","rtf","odt"};
    categories["Audio"] = {"mp3","wav","flac","aac","ogg","wma","m4a"};
    categories["Archives"] = {"zip","rar","7z","tar","gz","bz2"};
    categories["Code"] = {"cpp","h","c","py","java","js","html","css","php"};

    QMap<QString, quint64> categorySizes;
    quint64 otherSize = 0;

    for (const auto &file : allFiles) {
        bool matched = false;
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            if (it.value().contains(file.extension)) {
                categorySizes[it.key()] += file.size;
                matched = true;
                break;
            }
        }
        if (!matched) otherSize += file.size;
    }
    if (otherSize > 0) categorySizes["Others"] = otherSize;

    QPieSeries *series = new QPieSeries();
    quint64 totalSize = rootNode.size;

    for (auto it = categorySizes.begin(); it != categorySizes.end(); ++it) {
        double percent = (totalSize > 0) ? (100.0 * it.value() / totalSize) : 0.0;
        QPieSlice *slice = series->append(QString("%1 (%2%)").arg(it.key()).arg(percent, 0, 'f', 1), it.value());
        slice->setLabelVisible(true);
    }

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Storage Distribution by File Type");
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    QChartView *view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Storage by File Type (Pie Chart)");
    dialog->resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->addWidget(view);
    dialog->setLayout(layout);
    dialog->exec();
}
void MainWindow::collectAllFilesSmart(const FileNode &node, QVector<SmartFileInfo> &files)
{
    for (const FileNode &child : node.children) {
        if (!child.isFolder) {
            SmartFileInfo info;
            info.name = child.name;
            info.path = child.path;
            info.size = child.size;
            info.lastModified = child.lastModified;

            int dotIdx = child.name.lastIndexOf('.');
            info.extension = (dotIdx > 0) ? child.name.mid(dotIdx + 1).toLower() : "noext";

            files.append(info);
        }
        if (child.isFolder)
            collectAllFilesSmart(child, files);
    }
}
