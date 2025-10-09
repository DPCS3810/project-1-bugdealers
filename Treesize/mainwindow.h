#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QDateTime>

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

private slots:
    void onScanClicked();

};
#endif // MAINWINDOW_H
