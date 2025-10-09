#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QFileInfo>
#include <QTreeWidgetItem>
#include <QDebug>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    ui->labelCurrentDir->setText("Current Directory: —");


    // Initialize TreeWidget only (no scan yet)
    ui->treeWidget->setColumnCount(5);
    QStringList headers = {"Name", "Size (bytes)", "% of Parent", "Last Modified", "File Count"};
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
