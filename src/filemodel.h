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

#ifndef FILEMODEL_H
#define FILEMODEL_H

#include <QAbstractListModel>
#include <QDir>
#include <QFileSystemWatcher>
#include "statfileinfo.h"
#include <QMimeDatabase>

/**
 * @brief The FileModel class can be used as a model in a ListView to display a list of files
 * in the current directory. It has methods to change the current directory and to access
 * file info.
 * It also actively monitors the directory. If the directory changes, then the model is
 * updated automatically if active is true. If active is false, then the directory is
 * updated when active becomes true.
 */
class FileModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(Sort sortBy READ sortBy WRITE setSortBy NOTIFY sortByChanged)
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)
    Q_PROPERTY(Qt::CaseSensitivity caseSensitivity READ caseSensitivity WRITE setCaseSensitivity NOTIFY caseSensitivityChanged)
    Q_PROPERTY(bool includeDirectories READ includeDirectories WRITE setIncludeDirectories NOTIFY includeFoldersChanged)
    Q_PROPERTY(DirectorySort directorySort READ directorySort WRITE setDirectorySort NOTIFY directorySortChanged)
    Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters NOTIFY nameFiltersChanged)
    Q_PROPERTY(bool populated READ populated NOTIFY populatedChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)

    Q_ENUMS(Error)
    Q_ENUMS(Sort)
    Q_ENUMS(DirectorySort)

public:
    enum Error {
        NoError,
        ErrorReadNoPermissions
    };

    enum Sort {
        SortByName,
        SortByModified,
        SortBySize,
        SortByExtension
    };

    enum DirectorySort {
        SortDirectoriesWithFiles,
        SortDirectoriesBeforeFiles,
        SortDirectoriesAfterFiles
    };

    explicit FileModel(QObject *parent = 0);
    ~FileModel();

    // methods needed by ListView
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QHash<int, QByteArray> roleNames() const;

    // property accessors
    QString path() const { return m_path; }
    void setPath(QString path);

    Sort sortBy() const { return m_sortBy; }
    void setSortBy(Sort sortBy);

    Qt::SortOrder sortOrder() const { return m_sortOrder; }
    void setSortOrder(Qt::SortOrder order);

    Qt::CaseSensitivity caseSensitivity() const { return m_caseSensitivity; }
    void setCaseSensitivity(Qt::CaseSensitivity sensitivity);

    bool includeDirectories() const { return m_includeDirectories; }
    void setIncludeDirectories(bool include);

    DirectorySort directorySort() const { return m_directorySort; }
    void setDirectorySort(DirectorySort sort);

    QStringList nameFilters() const { return m_nameFilters; }
    void setNameFilters(const QStringList &filters);

    bool populated() const { return m_populated; }
    int count() const;

    bool active() const { return m_active; }
    void setActive(bool active);

    int selectedCount() const { return m_selectedCount; }

    // methods accessible from QML
    Q_INVOKABLE QString appendPath(QString pathName);
    Q_INVOKABLE QString parentPath();
    Q_INVOKABLE QString fileNameAt(int fileIndex);

    // file selection
    Q_INVOKABLE void toggleSelectedFile(int fileIndex);
    Q_INVOKABLE void clearSelectedFiles();
    Q_INVOKABLE void selectAllFiles();
    Q_INVOKABLE QStringList selectedFiles() const;

public slots:
    // reads the directory and inserts/removes model items as needed
    Q_INVOKABLE void refresh();
    // reads the directory and sets all model items
    Q_INVOKABLE void refreshFull();

signals:
    void pathChanged();
    void sortByChanged();
    void sortOrderChanged();
    void caseSensitivityChanged();
    void includeFoldersChanged();
    void directorySortChanged();
    void nameFiltersChanged();
    void populatedChanged();
    void countChanged();
    void error(Error error, QString fileName);
    void activeChanged();
    void selectedCountChanged();

private slots:
    void readDirectory();

private:
    void recountSelectedFiles();
    void readAllEntries();
    void refreshEntries();
    void clearModel();
    bool filesContains(const QList<StatFileInfo> &files, const StatFileInfo &fileData) const;

    QDir directory() const;

    QString m_path;
    Sort m_sortBy;
    DirectorySort m_directorySort;
    Qt::SortOrder m_sortOrder;
    Qt::CaseSensitivity m_caseSensitivity;
    bool m_includeDirectories;
    bool m_active;
    bool m_dirty;
    bool m_populated;
    int m_selectedCount;
    QStringList m_nameFilters;
    QList<StatFileInfo> m_files;
    QFileSystemWatcher *m_watcher;
    QMimeDatabase m_mimeDatabase;
};



#endif // FILEMODEL_H
