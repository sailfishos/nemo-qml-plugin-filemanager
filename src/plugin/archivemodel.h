/*
 * Copyright (C) 2018 Jolla Ltd.
 * Contact: Raine Mäkeläinen <raine.makelainen@jolla.com>
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

#ifndef SAILFISH_ARCHIVE_LISTMODEL_H
#define SAILFISH_ARCHIVE_LISTMODEL_H

#include <QAbstractListModel>

namespace Sailfish {

class ArchiveModelPrivate;

class ArchiveModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(ErrorState errorState READ errorState NOTIFY errorStateChanged)
    Q_PROPERTY(bool populated READ populated NOTIFY populatedChanged)
    Q_PROPERTY(bool extracting READ extracting NOTIFY extractingChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool autoRename READ autoRename WRITE setAutoRename NOTIFY autoRenameChanged)
    Q_PROPERTY(QString archiveFile READ archiveFile WRITE setArchiveFile NOTIFY archiveFileChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(QString baseName READ baseName NOTIFY baseNameChanged)
    Q_PROPERTY(QString completeSuffix READ completeSuffix NOTIFY completeSuffixChanged)
    Q_PROPERTY(qint64 requiredSpace READ requiredSpace NOTIFY requiredSpaceChanged)

public:
    explicit ArchiveModel(QObject *parent = nullptr);
    ~ArchiveModel();

    enum Status {
        Null,
        Loading,
        Ready,
        Extracting,
        Error
    };
    Q_ENUM(Status)

    enum ErrorState {
        NoError,
        ErrorArchiveFileNoSet,
        ErrorArchiveNotFound,
        ErrorArchiveOpenFailed,
        ErrorArchiveExtractFailed,
        ErrorInvalidArchivePath,
        ErrorInvalidArchiveEntry,
        ErrorExtractingInProgress,
        ErrorUnsupportedArchiveFormat
    };
    Q_ENUM(ErrorState)

    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;

    QString path() const;
    void setPath(const QString &path);

    Status status() const;
    ErrorState errorState();
    bool populated() const;
    bool extracting() const;
    int count() const;

    bool active() const;
    void setActive(bool active);

    bool autoRename() const;
    void setAutoRename(bool autoRename);

    QString archiveFile() const;
    void setArchiveFile(const QString &archiveFile);

    QString fileName() const;
    QString baseName() const;
    QString completeSuffix() const;

    qint64 requiredSpace() const;

    Q_INVOKABLE QString appendPath(const QString &pathName) const;
    Q_INVOKABLE QString errorString() const;

    Q_INVOKABLE bool extractAllFiles(const QString &targetPath);
    Q_INVOKABLE bool extractFile(const QString &entryName, const QString &targetPath);

    // If component user deletes the file from outside, clean state for the extracted.
    Q_INVOKABLE bool cleanExtractedEntry(const QString &entry);

signals:
    void pathChanged();
    void statusChanged();
    void errorStateChanged();
    void populatedChanged();
    void extractingChanged();
    void countChanged();
    void activeChanged();
    void autoRenameChanged();
    void archiveFileChanged();
    void fileNameChanged();
    void baseNameChanged();
    void completeSuffixChanged();
    void requiredSpaceChanged();

    void filesExtracted(const QString &path, bool isDir, const QString &entryName, const QStringList &entries);

private:
    ArchiveModelPrivate *d;
    Q_DISABLE_COPY(ArchiveModel)
};

}

#endif // SAILFISH_ARCHIVE_LISTMODEL_H
