#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QDateTime>
#include <QElapsedTimer>

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

    void countTotalItems(const QString &path); // helper
    QString currentDirPath;

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


};
#endif // MAINWINDOW_H
