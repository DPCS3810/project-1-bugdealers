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
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = ui->treeWidget->topLevelItem(i);
        filterTree(item, query);
    }
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

// ================== FREE SPACE ANALYSIS FUNCTIONS ==================

void MainWindow::onFreeSpaceClicked()
{
    // Check if a directory has been scanned
    if (currentDirPath.isEmpty()) {
        QMessageBox::information(this, "Free Space Analysis",
                                 "Please scan a directory first before analyzing free space options.");
        return;
    }

    QMenu menu(this);
    menu.setTitle("Free Space Options");

    QAction *top5Act = menu.addAction("View Top 5 Biggest Files");
    QAction *duplicatesAct = menu.addAction("Check for Possible Duplicates");
    QAction *lastModifiedAct = menu.addAction("Check by Last Modified Date");
    QAction *formatAct = menu.addAction("Display by Format");

    connect(top5Act, &QAction::triggered, this, &MainWindow::showTop5BiggestFiles);
    connect(duplicatesAct, &QAction::triggered, this, &MainWindow::checkForDuplicates);
    connect(lastModifiedAct, &QAction::triggered, this, &MainWindow::checkByLastModified);
    connect(formatAct, &QAction::triggered, this, &MainWindow::displayByFormat);

    menu.exec(ui->pushButton_10->mapToGlobal(QPoint(0, ui->pushButton_10->height())));
}

void MainWindow::collectAllFiles(const FileNode &node, QVector<FileInfo> &files)
{
    for (const FileNode &child : node.children) {
        if (!child.isFolder) {
            // It's a file
            FileInfo info;
            info.name = child.name;
            info.path = child.path;
            info.size = child.size;
            info.lastModified = child.lastModified;

            // Extract extension
            int dotIdx = child.name.lastIndexOf('.');
            if (dotIdx > 0) {
                info.extension = child.name.mid(dotIdx + 1).toLower();
            } else {
                info.extension = "no extension";
            }

            files.append(info);
        }

        // Recurse into folders
        if (child.isFolder) {
            collectAllFiles(child, files);
        }
    }
}

void MainWindow::showTop5BiggestFiles()
{
    if (rootNode.size == 0) {
        QMessageBox::information(this, "Top 5 Biggest Files",
                                 "No files found in the scanned directory.");
        return;
    }

    // Collect all files
    QVector<FileInfo> allFiles;
    collectAllFiles(rootNode, allFiles);

    if (allFiles.isEmpty()) {
        QMessageBox::information(this, "Top 5 Biggest Files",
                                 "No files found in the scanned directory.");
        return;
    }

    // Sort by size (descending)
    std::sort(allFiles.begin(), allFiles.end(), [](const FileInfo &a, const FileInfo &b) {
        return a.size > b.size;
    });

    // Take top 5
    int count = std::min(5, static_cast<int>(allFiles.size()));


    QString message = "These are the top " + QString::number(count) +
                      " files that take up the most storage in this directory.\n"
                      "You might want to consider handling these if you would like to free up space:\n\n";

    quint64 totalSize = rootNode.size;

    for (int i = 0; i < count; ++i) {
        const FileInfo &file = allFiles[i];
        double percentage = (totalSize > 0) ? (100.0 * file.size / totalSize) : 0.0;

        // Format size
        QString sizeStr;
        double val = file.size;
        if (val >= 1024.0 * 1024.0 * 1024.0) {
            sizeStr = QString::number(val / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
        } else if (val >= 1024.0 * 1024.0) {
            sizeStr = QString::number(val / (1024.0 * 1024.0), 'f', 2) + " MB";
        } else if (val >= 1024.0) {
            sizeStr = QString::number(val / 1024.0, 'f', 2) + " KB";
        } else {
            sizeStr = QString::number(val, 'f', 0) + " bytes";
        }

        message += QString("%1. %2\n   Size: %3 (%4% of total)\n   Path: %5\n\n")
                       .arg(i + 1)
                       .arg(file.name)
                       .arg(sizeStr)
                       .arg(percentage, 0, 'f', 2)
                       .arg(file.path);
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Top 5 Biggest Files");
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

void MainWindow::checkForDuplicates()
{
    // Collect all files
    QVector<FileInfo> allFiles;
    collectAllFiles(rootNode, allFiles);

    if (allFiles.isEmpty()) {
        QMessageBox::information(this, "Check for Duplicates",
                                 "No files found in the scanned directory.");
        return;
    }

    // Group files by size
    QMap<quint64, QVector<FileInfo>> sizeGroups;
    for (const FileInfo &file : allFiles) {
        if (file.size > 0) {  // Ignore 0-byte files
            sizeGroups[file.size].append(file);
        }
    }

    // Find groups with more than one file
    QString message;
    int duplicateGroupCount = 0;

    for (auto it = sizeGroups.begin(); it != sizeGroups.end(); ++it) {
        if (it.value().size() > 1) {
            duplicateGroupCount++;

            // Format size
            quint64 size = it.key();
            QString sizeStr;
            double val = size;
            if (val >= 1024.0 * 1024.0 * 1024.0) {
                sizeStr = QString::number(val / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
            } else if (val >= 1024.0 * 1024.0) {
                sizeStr = QString::number(val / (1024.0 * 1024.0), 'f', 2) + " MB";
            } else if (val >= 1024.0) {
                sizeStr = QString::number(val / 1024.0, 'f', 2) + " KB";
            } else {
                sizeStr = QString::number(val, 'f', 0) + " bytes";
            }

            message += QString("\nGroup %1 - Size: %2 (%3 files)\n")
                           .arg(duplicateGroupCount)
                           .arg(sizeStr)
                           .arg(it.value().size());

            for (const FileInfo &file : it.value()) {
                message += QString("  • %1\n").arg(file.path);
            }
        }
    }

    if (duplicateGroupCount == 0) {
        message = "No size matches were found in this directory.";
    } else {
        message = QString("These files have the exact same size, they might have the same content.\n"
                          "You might want to check once.\n\n"
                          "Found %1 groups of files with matching sizes:\n")
                      .arg(duplicateGroupCount) + message;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Check for Duplicates");
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

void MainWindow::checkByLastModified()
{
    // Collect all files
    QVector<FileInfo> allFiles;
    collectAllFiles(rootNode, allFiles);

    if (allFiles.isEmpty()) {
        QMessageBox::information(this, "Check by Last Modified",
                                 "No files found in the scanned directory.");
        return;
    }

    // Sort by last modified date (ascending - oldest first)
    std::sort(allFiles.begin(), allFiles.end(), [](const FileInfo &a, const FileInfo &b) {
        return a.lastModified < b.lastModified;
    });

    // Take top 5 oldest
    int count = std::min(5, static_cast<int>(allFiles.size()));


    QString message = "The following files have not been modified in a while.\n"
                      "They might be redundant:\n\n";

    QDateTime now = QDateTime::currentDateTime();

    for (int i = 0; i < count; ++i) {
        const FileInfo &file = allFiles[i];

        // Calculate days since modification
        qint64 daysSince = file.lastModified.daysTo(now);

        // Format size
        QString sizeStr;
        double val = file.size;
        if (val >= 1024.0 * 1024.0 * 1024.0) {
            sizeStr = QString::number(val / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
        } else if (val >= 1024.0 * 1024.0) {
            sizeStr = QString::number(val / (1024.0 * 1024.0), 'f', 2) + " MB";
        } else if (val >= 1024.0) {
            sizeStr = QString::number(val / 1024.0, 'f', 2) + " KB";
        } else {
            sizeStr = QString::number(val, 'f', 0) + " bytes";
        }

        message += QString("%1. %2\n   Size: %3\n   Last Modified: %4 (%5 days ago)\n   Path: %6\n\n")
                       .arg(i + 1)
                       .arg(file.name)
                       .arg(sizeStr)
                       .arg(file.lastModified.toString("yyyy-MM-dd hh:mm"))
                       .arg(daysSince)
                       .arg(file.path);
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Check by Last Modified Date");
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

void MainWindow::displayByFormat()
{
    // Collect all files
    QVector<FileInfo> allFiles;
    collectAllFiles(rootNode, allFiles);

    if (allFiles.isEmpty()) {
        QMessageBox::information(this, "Display by Format",
                                 "No files found in the scanned directory.");
        return;
    }

    // Define format categories
    QMap<QString, QStringList> categories;
    categories["Images"] = QStringList() << "jpg" << "jpeg" << "png" << "gif" << "bmp" << "svg" << "ico" << "webp";
    categories["Videos"] = QStringList() << "mp4" << "avi" << "mkv" << "mov" << "wmv" << "flv" << "webm" << "m4v";
    categories["Documents"] = QStringList() << "pdf" << "doc" << "docx" << "txt" << "rtf" << "odt";
    categories["Spreadsheets"] = QStringList() << "xls" << "xlsx" << "csv" << "ods";
    categories["Audio"] = QStringList() << "mp3" << "wav" << "flac" << "aac" << "ogg" << "wma" << "m4a";
    categories["Archives"] = QStringList() << "zip" << "rar" << "7z" << "tar" << "gz" << "bz2";
    categories["Code"] = QStringList() << "cpp" << "h" << "c" << "py" << "java" << "js" << "html" << "css" << "php";

    // Categorize files and calculate sizes
    QMap<QString, quint64> categorySizes;
    quint64 otherSize = 0;

    for (const FileInfo &file : allFiles) {
        QString ext = file.extension;
        bool categorized = false;

        for (auto it = categories.begin(); it != categories.end(); ++it) {
            if (it.value().contains(ext)) {
                categorySizes[it.key()] += file.size;
                categorized = true;
                break;
            }
        }

        if (!categorized) {
            otherSize += file.size;
        }
    }

    if (otherSize > 0) {
        categorySizes["Others"] = otherSize;
    }

    // Create bar chart
    QBarSet *set = new QBarSet("Size");
    QStringList categoryNames;
    quint64 totalSize = rootNode.size;

    // Sort categories by size (descending)
    QList<QPair<QString, quint64>> sortedCategories;
    for (auto it = categorySizes.begin(); it != categorySizes.end(); ++it) {
        sortedCategories.append(qMakePair(it.key(), it.value()));
    }
    std::sort(sortedCategories.begin(), sortedCategories.end(),
              [](const QPair<QString, quint64> &a, const QPair<QString, quint64> &b) {
                  return a.second > b.second;
              });

    // Add to chart
    for (const auto &pair : sortedCategories) {
        double sizeMB = pair.second / (1024.0 * 1024.0);
        *set << sizeMB;

        double percentage = (totalSize > 0) ? (100.0 * pair.second / totalSize) : 0.0;
        categoryNames << QString("%1\n(%2%)").arg(pair.first).arg(percentage, 0, 'f', 1);
    }

    QBarSeries *series = new QBarSeries();
    series->append(set);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Storage Distribution by File Format");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categoryNames);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Size (MB)");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setVisible(false);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // Create dialog
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Storage Distribution by Format");
    dialog->resize(900, 600);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    layout->addWidget(chartView);

    // Add summary text
    QString summary = "\nDetailed Breakdown:\n\n";
    for (const auto &pair : sortedCategories) {
        double percentage = (totalSize > 0) ? (100.0 * pair.second / totalSize) : 0.0;

        QString sizeStr;
        double val = pair.second;
        if (val >= 1024.0 * 1024.0 * 1024.0) {
            sizeStr = QString::number(val / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
        } else if (val >= 1024.0 * 1024.0) {
            sizeStr = QString::number(val / (1024.0 * 1024.0), 'f', 2) + " MB";
        } else if (val >= 1024.0) {
            sizeStr = QString::number(val / 1024.0, 'f', 2) + " KB";
        } else {
            sizeStr = QString::number(val, 'f', 0) + " bytes";
        }

        summary += QString("%1: %2 (%3%)\n")
                       .arg(pair.first)
                       .arg(sizeStr)
                       .arg(percentage, 0, 'f', 2);
    }

    QLabel *summaryLabel = new QLabel(summary);
    summaryLabel->setWordWrap(true);
    layout->addWidget(summaryLabel);

    dialog->setLayout(layout);
    dialog->exec();
}
