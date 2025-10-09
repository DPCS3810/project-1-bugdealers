#include "mainwindow.h"
#include "ui_mainwindow.h"
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


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("0%");

    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(ui->pushButton_7, &QPushButton::clicked, this, &MainWindow::onTextViewSettingsClicked);
    connect(ui->pushButton_4, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    connect(ui->pushButton_3, &QPushButton::clicked, this, &MainWindow::onFilterClicked);
    connect(ui->pushButton_9, &QPushButton::clicked, this, &MainWindow::onReScanClicked);
    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::onCancelScanClicked);

    ui->labelCurrentDir->setText("Current Directory: —");


    // Initialize TreeWidget only (no scan yet)
    ui->treeWidget->setColumnCount(5);
    QStringList headers = {"Name", "Size", "% of Parent", "Last Modified", "File Count"};
    ui->treeWidget->setHeaderLabels(headers);
    ui->treeWidget->setSortingEnabled(true);
    ui->treeWidget->sortByColumn(0, Qt::AscendingOrder); // optional: default sort by Name

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

    // Create root node
    FileNode rootNode;
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
    rootItem->setText(2, "100%");
    rootItem->setText(3, rootNode.lastModified.toString("yyyy-MM-dd hh:mm"));
    rootItem->setText(4, QString::number(rootNode.fileCount));

    populateTree(rootNode, rootItem);
    ui->treeWidget->addTopLevelItem(rootItem);
    ui->treeWidget->topLevelItem(0)->setExpanded(true);
    updateTreeDisplay();

    // Set progress to 100% at the end
    ui->progressBar->setValue(100);
    ui->progressBar->setFormat("100% (Done)");
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
    // Lambda to format size
    auto formatSize = [this](quint64 size) {
        double val = size;
        QString suffix = " bytes";
        switch (currentUnit) {
        case KB: val = size / 1024.0; suffix = " KB"; break;
        case MB: val = size / (1024.0 * 1024.0); suffix = " MB"; break;
        case GB: val = size / (1024.0 * 1024.0 * 1024.0); suffix = " GB"; break;
        default: break;
        }
        return QString::number(val, 'f', 2) + suffix;
    };

    // Update every item recursively
    std::function<void(QTreeWidgetItem*)> updateItem = [&](QTreeWidgetItem *item) {
        if (!item) return;
        bool ok;
        quint64 size = item->text(1).split(" ").first().toDouble(&ok);
        if (ok) item->setText(1, formatSize(size));

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

    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i)
        filterTreeBySize(ui->treeWidget->topLevelItem(i), value, greater);
}

bool MainWindow::filterTreeBySize(QTreeWidgetItem *item, double size, bool greater)
{
    // Parse item size (strip suffix)
    QString text = item->text(1);
    double itemSize = text.split(" ").first().toDouble();

    bool match = greater ? (itemSize > size) : (itemSize < size);

    bool childMatch = false;
    for (int i = 0; i < item->childCount(); ++i) {
        bool childVisible = filterTreeBySize(item->child(i), size, greater);
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
        // Size
        item->setText(1, QString::number(child.size));
        // Percentage of parent
        double percent = (node.size > 0) ? (100.0 * child.size / node.size) : 0.0;
        item->setText(2, QString::number(percent, 'f', 2) + "%");
        // Last modified
        item->setText(3, child.lastModified.toString("yyyy-MM-dd hh:mm"));
        // File count
        item->setText(4, QString::number(child.fileCount));

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

    FileNode rootNode;
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

