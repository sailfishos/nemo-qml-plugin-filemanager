/*
 * Copyright (C) 2016 Jolla Ltd.
 * Contact: Joona Petrell <joona.petrell@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Jolla Ltd. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "filemodel.h"

#include <QDateTime>
#include <QDebug>
#include <QGuiApplication>
#include <QMimeType>
#include <QQmlInfo>

#include <unistd.h>

namespace {

enum {
    FileNameRole = Qt::UserRole + 1,
    MimeTypeRole = Qt::UserRole + 2,
    SizeRole = Qt::UserRole + 3,
    LastModifiedRole = Qt::UserRole + 4,
    CreatedRole = Qt::UserRole + 5,
    IsDirRole = Qt::UserRole + 6,
    IsLinkRole = Qt::UserRole + 7,
    SymLinkTargetRole = Qt::UserRole + 8,
    IsSelectedRole = Qt::UserRole + 9,
    ExtensionRole = Qt::UserRole + 10,
    AbsolutePathRole = Qt::UserRole + 11
};

int access(QString fileName, int how)
{
    QByteArray fab = fileName.toUtf8();
    char *fn = fab.data();
    return ::access(fn, how);
}

QList<StatFileInfo> directoryEntries(const QDir &dir)
{
    QList<StatFileInfo> rv;

    QStringList fileList = dir.entryList();
    foreach (const QString &fileName, fileList) {
        if (fileName.startsWith("qt_temp.")) {
            // Workaround for QFile::copy() creating intermediate qt_temp.* file (see QTBUG-27601)
            continue;
        }
        QString fullpath = dir.absoluteFilePath(fileName);
        StatFileInfo info(fullpath);
        rv.append(info);
    }

    return rv;
}

}

FileModel::FileModel(QObject *parent) :
    QAbstractListModel(parent),
    m_sortBy(SortByName),
    m_directorySort(SortDirectoriesWithFiles),
    m_sortOrder(Qt::AscendingOrder),
    m_caseSensitivity(Qt::CaseSensitive),
    m_includeDirectories(true),
    m_active(false),
    m_dirty(false),
    m_populated(false),
    m_selectedCount(0)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, SIGNAL(directoryChanged(const QString&)), this, SLOT(refresh()));
    connect(m_watcher, SIGNAL(fileChanged(const QString&)), this, SLOT(refresh()));
}

FileModel::~FileModel()
{
}

int FileModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_files.count();
}

QVariant FileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() > m_files.size()-1)
        return QVariant();

    const StatFileInfo &info = m_files.at(index.row());
    switch (role) {

    case Qt::DisplayRole:
    case FileNameRole:
        return info.fileName();

    case MimeTypeRole:
        return m_mimeDatabase.mimeTypeForFile(info.absoluteFilePath()).name();

    case SizeRole:
        return info.size();

    case LastModifiedRole:
        return info.lastModified();

    case CreatedRole:
        return info.created();

    case IsDirRole:
        return info.isDirAtEnd();

    case IsLinkRole:
        return info.isSymLink();

    case SymLinkTargetRole:
        return info.symLinkTarget();

    case IsSelectedRole:
        return info.isSelected();

    case ExtensionRole:
        return info.extension();

    case AbsolutePathRole:
        return info.absoluteFilePath();

    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FileModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert(FileNameRole, QByteArray("fileName"));
    roles.insert(MimeTypeRole, QByteArray("mimeType"));
    roles.insert(SizeRole, QByteArray("size"));
    roles.insert(LastModifiedRole, QByteArray("modified"));
    roles.insert(CreatedRole, QByteArray("created"));
    roles.insert(IsDirRole, QByteArray("isDir"));
    roles.insert(IsLinkRole, QByteArray("isLink"));
    roles.insert(SymLinkTargetRole, QByteArray("symLinkTarget"));
    roles.insert(IsSelectedRole, QByteArray("isSelected"));
    roles.insert(ExtensionRole, QByteArray("extension"));
    roles.insert(AbsolutePathRole, QByteArray("absolutePath"));
    return roles;
}

int FileModel::count() const
{
    return m_files.count();
}

void FileModel::setPath(QString path)
{
    if (m_path == path)
        return;

    if (m_populated) {
        m_populated = false;
        emit populatedChanged();
    }

    // update watcher to watch the new directory
    if (!m_path.isEmpty())
        m_watcher->removePath(m_path);

    if (!path.isEmpty())
        m_watcher->addPath(path);

    m_path = path;

    readDirectory();

    m_dirty = false;

    emit pathChanged();

    if (!m_populated) {
        m_populated = true;
        emit populatedChanged();
    }
}

void FileModel::setSortBy(Sort sortBy)
{
    if (m_sortBy == sortBy)
        return;

    m_sortBy = sortBy;
    emit sortByChanged();

    refreshEntries();
    m_dirty = false;
}

void FileModel::setSortOrder(Qt::SortOrder order)
{
    if (m_sortOrder == order)
        return;

    m_sortOrder = order;
    emit sortOrderChanged();

    refreshEntries();
    m_dirty = false;
}

void FileModel::setCaseSensitivity(Qt::CaseSensitivity sensitivity)
{
    if (m_caseSensitivity == sensitivity)
        return;

    m_caseSensitivity = sensitivity;
    emit caseSensitivityChanged();

    refreshEntries();
    m_dirty = false;
}

void FileModel::setIncludeDirectories(bool include)
{
    if (m_includeDirectories == include)
        return;

    m_includeDirectories = include;
    emit includeFoldersChanged();

    refreshEntries();
    m_dirty = false;
}

void FileModel::setDirectorySort(DirectorySort sort)
{
    if (m_directorySort == sort)
        return;

    m_directorySort = sort;
    emit directorySortChanged();

    refreshEntries();
    m_dirty = false;
}

void FileModel::setNameFilters(const QStringList &filters)
{
    if (m_nameFilters == filters)
        return;

    m_nameFilters = filters;
    emit nameFiltersChanged();

    refreshEntries();
    m_dirty = false;
}

void FileModel::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
    emit activeChanged();

    if (m_dirty)
        readDirectory();

    m_dirty = false;
}

QString FileModel::appendPath(QString pathName)
{
    return QDir::cleanPath(QDir(m_path).absoluteFilePath(pathName));
}

QString FileModel::parentPath()
{
    return QDir::cleanPath(QDir(m_path).absoluteFilePath(".."));
}

QString FileModel::fileNameAt(int fileIndex)
{
    if (fileIndex < 0 || fileIndex >= m_files.count())
        return QString();

    return m_files.at(fileIndex).absoluteFilePath();
}

void FileModel::toggleSelectedFile(int fileIndex)
{
    if (!m_files.at(fileIndex).isSelected()) {
        StatFileInfo info = m_files.at(fileIndex);
        info.setSelected(true);
        m_files[fileIndex] = info;
        m_selectedCount++;
    } else {
        StatFileInfo info = m_files.at(fileIndex);
        info.setSelected(false);
        m_files[fileIndex] = info;
        m_selectedCount--;
    }
    // emit signal for views
    QModelIndex topLeft = index(fileIndex, 0);
    QModelIndex bottomRight = index(fileIndex, 0);
    emit dataChanged(topLeft, bottomRight);

    emit selectedCountChanged();
}

void FileModel::clearSelectedFiles()
{
    QMutableListIterator<StatFileInfo> iter(m_files);
    int row = 0;
    while (iter.hasNext()) {
        StatFileInfo &info = iter.next();
        info.setSelected(false);
        // emit signal for views
        QModelIndex topLeft = index(row, 0);
        QModelIndex bottomRight = index(row, 0);
        emit dataChanged(topLeft, bottomRight);
        row++;
    }
    m_selectedCount = 0;
    emit selectedCountChanged();
}

void FileModel::selectAllFiles()
{
    QMutableListIterator<StatFileInfo> iter(m_files);
    int row = 0;
    while (iter.hasNext()) {
        StatFileInfo &info = iter.next();
        info.setSelected(true);
        // emit signal for views
        QModelIndex topLeft = index(row, 0);
        QModelIndex bottomRight = index(row, 0);
        emit dataChanged(topLeft, bottomRight);
        row++;
    }
    m_selectedCount = m_files.count();
    emit selectedCountChanged();
}

QStringList FileModel::selectedFiles() const
{
    if (m_selectedCount == 0)
        return QStringList();

    QStringList fileNames;
    foreach (const StatFileInfo &info, m_files) {
        if (info.isSelected())
            fileNames.append(info.absoluteFilePath());
    }
    return fileNames;
}

void FileModel::refresh()
{
    if (!m_active) {
        m_dirty = true;
        return;
    }

    refreshEntries();
    m_dirty = false;
}

void FileModel::refreshFull()
{
    if (!m_active) {
        m_dirty = true;
        return;
    }

    readDirectory();
    m_dirty = false;
}

void FileModel::readDirectory()
{
    // wrapped in reset model methods to get views notified
    beginResetModel();

    m_files.clear();

    if (!m_path.isEmpty())
        readAllEntries();

    endResetModel();
    emit countChanged();
    recountSelectedFiles();
}

void FileModel::recountSelectedFiles()
{
    int count = 0;
    foreach (const StatFileInfo &info, m_files) {
        if (info.isSelected())
            count++;
    }
    if (m_selectedCount != count) {
        m_selectedCount = count;
        emit selectedCountChanged();
    }
}

void FileModel::readAllEntries()
{
    QDir dir(directory());
    if (!dir.exists()) {
        qmlInfo(this) << "Path " << dir.path() << " not found";
        return;
    }

    if (access(dir.path(), R_OK) == -1) {
        qmlInfo(this) << "No permissions to access " << dir.path();
        emit error(ErrorReadNoPermissions, dir.path());
        return;
    }

    m_files = directoryEntries(dir);
}

void FileModel::refreshEntries()
{
    // empty path
    if (m_path.isEmpty()) {
        clearModel();
        return;
    }

    QDir dir(directory());
    if (!dir.exists()) {
        clearModel();

        qmlInfo(this) << "Path " << dir.path() << " not found";
        return;
    }

    if (access(dir.path(), R_OK) == -1) {
        clearModel();
        qmlInfo(this) << "No permissions to access " << dir.path();
        emit error(ErrorReadNoPermissions, dir.path());
        return;
    }

    int oldCount = m_files.count();

    // read all files
    QList<StatFileInfo> newFiles = directoryEntries(dir);

    ::synchronizeList(this, m_files, newFiles);

    if (m_files.count() != oldCount)
        emit countChanged();
    recountSelectedFiles();
}

int FileModel::insertRange(int index, int count, const QList<StatFileInfo> &source, int sourceIndex)
{
    beginInsertRows(QModelIndex(), index, index + count - 1);

    for (int i = 0; i < count; ++i)
        m_files.insert(index + i, source.at(sourceIndex + i));

    endInsertRows();
    return count;
}

int FileModel::removeRange(int index, int count)
{
    beginRemoveRows(QModelIndex(), index, index + count - 1);

    QList<StatFileInfo>::iterator it = m_files.begin() + index;
    m_files.erase(it, it + count);

    endRemoveRows();
    return 0;
}

void FileModel::clearModel()
{
    beginResetModel();
    m_files.clear();
    endResetModel();
    emit countChanged();
}

QDir FileModel::directory() const
{
    QDir dir(m_path);
    if (dir.exists()) {
        QDir::Filters filters(QDir::Files | QDir::NoDotAndDotDot | QDir::System);

        if (m_includeDirectories) {
            filters |= QDir::AllDirs;
        }

        QDir::SortFlags sortFlags(QDir::LocaleAware);

        if (m_sortBy == SortByName) {
            sortFlags |= QDir::Name;
        } else if (m_sortBy == SortByModified) {
            sortFlags |= QDir::Time;
        } else if (m_sortBy == SortBySize) {
            sortFlags |= QDir::Size;
        } else if (m_sortBy == SortByExtension) {
            sortFlags |= QDir::Type;
        }

        if (m_sortOrder == Qt::DescendingOrder) {
            sortFlags |= QDir::Reversed;
        }
        if (m_caseSensitivity == Qt::CaseInsensitive) {
            sortFlags |= QDir::IgnoreCase;
        }

        if (m_directorySort == SortDirectoriesBeforeFiles) {
            sortFlags |= QDir::DirsFirst;
        } else if (m_directorySort == SortDirectoriesAfterFiles) {
            sortFlags |= QDir::DirsLast;
        }

        dir.setFilter(filters);
        dir.setSorting(sortFlags);

        if (!m_nameFilters.isEmpty()) {
            dir.setNameFilters(m_nameFilters);
        }
    }

    return dir;
}

