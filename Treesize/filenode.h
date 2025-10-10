// filenode.h

#pragma once
#include <QString>
#include <QList>
#include <QDateTime>
#include <cstdint>

struct FileNode {
    QString name;
    QString path;
    quint64 size = 0;
    bool isFolder = false;
    quint64 fileCount = 0;
    QList<FileNode> children;
    QDateTime lastModified;
};
