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
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)

    Q_ENUMS(Error)

public:
    enum Error {
        NoError,
        ErrorReadNoPermissions
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

    QString m_path;
    QList<StatFileInfo> m_files;
    int m_selectedCount;
    bool m_active;
    bool m_dirty;
    QFileSystemWatcher *m_watcher;
    QMimeDatabase m_mimeDatabase;
};



#endif // FILEMODEL_H
