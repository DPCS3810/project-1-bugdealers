#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

struct FileNode {
    QString name;
    QString path;
    quint64 size = 0;
    bool isFolder = false;
    quint64 fileCount = 0;
    QList<FileNode> children;
    QDateTime lastModified;

};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    void scanDirectory(const QString &path, FileNode &node);
    void populateTree(const FileNode &node, QTreeWidgetItem *parentItem);
    enum SizeUnit { BYTES, KB, MB, GB };
    SizeUnit currentUnit = BYTES;
    void updateTreeDisplay();  // refresh display after settings change
    QLineEdit *searchBox = nullptr;
    bool filterTree(QTreeWidgetItem *item, const QString &query);
    bool filterTreeBySize(QTreeWidgetItem *item, double size, bool greater);
    bool filterTreeByCount(QTreeWidgetItem *item, quint64 count, bool greater);
    bool filterTreeByFormat(QTreeWidgetItem *item, const QStringList &extList);

    quint64 totalItems = 0;   // total files/folders to scan
    quint64 scannedItems = 0;  // progress count
    QElapsedTimer timer;       // track elapsed time

    bool moveToRecycleBin(const QString &path);
    void countTotalItems(const QString &path); // helper
    QString currentDirPath;
    bool cancelScan = false; // track if the scan was canceled
    FileNode* findNodeByPath(FileNode &node, const QString &path);
    FileNode rootNode;  // Store the root node for later access


private slots:
    void onScanClicked();
    void onTextViewSettingsClicked();
    void onFontSizeSelected();
    void onSearchClicked();
    void onSearchTextChanged(const QString &text);
    void onFilterClicked();
    void filterBySize();
    void filterByFileCount();
    void filterByFormat();
    void resetFilter();
    void onReScanClicked();
    void onCancelScanClicked();
    void onExportClicked();
    void exportAsPDF();
    void exportAsJSON();
    void exportAsCSV();
    void onTreeItemCustomContextMenu(const QPoint &pos);
    void showFullPath(QTreeWidgetItem *item);
    void showPieChart(QTreeWidgetItem *item);
    void deleteItem(QTreeWidgetItem *item);
    void renameItem(QTreeWidgetItem *item);


};
#endif // MAINWINDOW_H
