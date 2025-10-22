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
#include "filenode.h"
#include <QCryptographicHash>
#include <QTextBrowser>
#include <QDialog>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TreeMapWidget;  // forward declaration

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
    bool filterTreeBySize(QTreeWidgetItem *item, quint64 sizeBytes, bool greater);
    bool filterTreeByCount(QTreeWidgetItem *item, quint64 count, bool greater);
    bool filterTreeByFormat(QTreeWidgetItem *item, const QStringList &extList);

    bool filterAndHighlightTree(QTreeWidgetItem *item, const QString &query);
    void clearHighlight(QTreeWidgetItem *item);

    //bool filterTreeByType(QTreeWidgetItem *item, const QStringList &extList);
    bool filterTreeHighlight(QTreeWidgetItem *item, std::function<bool(QTreeWidgetItem*)> matchFunc);

    quint64 totalItems = 0;   // total files/folders to scan
    quint64 scannedItems = 0;  // progress count
    QElapsedTimer timer;       // track elapsed time

    bool moveToRecycleBin(const QString &path);
    void countTotalItems(const QString &path); // helper
    QString currentDirPath;
    bool cancelScan = false; // track if the scan was canceled
    FileNode* findNodeByPath(FileNode &node, const QString &path);
    FileNode rootNode;  // Store the root node for later access
    TreeMapWidget *treeMapWidget;       // pointer to your custom widget
    bool graphicalViewEnabled;

    // Existing FileInfo struct (kept as-is)
    struct FileInfo {
        QString name;
        QString path;
        quint64 size;
        QDateTime lastModified;
        QString extension;
    };

    // NEW: Smart-aware file record including per-factor scores and partial hash
    struct SmartFileInfo {
        QString name;
        QString path;
        quint64 size = 0;
        QDateTime lastModified;
        QString extension;
        QByteArray partialHash;     // partial hash (1MB front+end)
        double sizeScore = 0.0;
        double ageScore = 0.0;
        double typeScore = 0.0;
        double duplicateScore = 0.0;
        double totalScore = 0.0;
    };

    // Helper to collect all files from FileNode tree (existing)
    void collectAllFiles(const FileNode &node, QVector<FileInfo> &files);

    // NEW helper for smart collection (returns SmartFileInfo objects)
    void collectAllFilesSmart(const FileNode &node, QVector<SmartFileInfo> &files);

    // NEW - partial hashing for duplicate detection
    QByteArray computePartialHash(const QString &filePath, quint64 fileSize);

    // NEW features
    void findPotentialDuplicatesSmart();     // Use partial hashing to detect likely duplicates
    void showSmartDeletionSuggestions();     // Unified scoring + breakdown HTML view
    void showFileTypePieChart();             // Replace previous bar chart with pie chart

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
    void exportAsExcel();
    void onTreeItemCustomContextMenu(const QPoint &pos);
    void showFullPath(QTreeWidgetItem *item);
    void showPieChart(QTreeWidgetItem *item);
    void deleteItem(QTreeWidgetItem *item);
    void renameItem(QTreeWidgetItem *item);
    void onFreeSpaceClicked();
    void onMaxDepthSelected(int depth);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void filterByType();
};

#endif // MAINWINDOW_H
