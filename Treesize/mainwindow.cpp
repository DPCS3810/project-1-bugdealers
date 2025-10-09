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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(ui->pushButton_7, &QPushButton::clicked, this, &MainWindow::onTextViewSettingsClicked);

    ui->labelCurrentDir->setText("Current Directory: —");


    // Initialize TreeWidget only (no scan yet)
    ui->treeWidget->setColumnCount(5);
    QStringList headers = {"Name", "Size", "% of Parent", "Last Modified", "File Count"};
    ui->treeWidget->setHeaderLabels(headers);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// Recursive DFS scan
void MainWindow::scanDirectory(const QString &path, FileNode &node)
{
    QDir dir(path);
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

    for (QFileInfo entry : entries) {
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
    }
}


void MainWindow::onScanClicked()
{
    // Open file dialog to select a directory
    QString dirPath = QFileDialog::getExistingDirectory(this, "Select Folder to Scan", QDir::homePath());

    if (dirPath.isEmpty())
        return; // user cancelled

    // Clear any previous contents
    ui->treeWidget->clear();
    ui->labelCurrentDir->setText("Current Directory: " + dirPath);

    // Create root node
    FileNode rootNode;
    rootNode.name = dirPath;
    rootNode.path = dirPath;
    rootNode.isFolder = true;

    // Run the directory scan
    scanDirectory(dirPath, rootNode);

    // Populate TreeWidget
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(ui->treeWidget);
    rootItem->setText(0, rootNode.name);
    rootItem->setText(1, QString::number(rootNode.size));
    rootItem->setText(2, "100%");  // root folder is 100% of itself
    rootItem->setText(3, rootNode.lastModified.toString("yyyy-MM-dd hh:mm"));
    rootItem->setText(4, QString::number(rootNode.fileCount));


    populateTree(rootNode, rootItem);
    ui->treeWidget->addTopLevelItem(rootItem);
    //ui->treeWidget->expandAll();
    ui->treeWidget->topLevelItem(0)->setExpanded(true);
    updateTreeDisplay();
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
