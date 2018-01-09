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

#ifndef SAILFISH_ARCHIVE_LISTMODEL_P_H
#define SAILFISH_ARCHIVE_LISTMODEL_P_H

#include "archivemodel.h"
#include "archiveinfo.h"

#include <QMimeDatabase>
#include <QObject>
#include <QtConcurrent>

#include <KArchive>

namespace Sailfish {

enum ExtractionMode {
    SingleFile,
    Archive
    // There could be selected files
};

typedef QPair<ArchiveModel::ErrorState, QString> FileExtractionResult;

class ArchiveModelPrivate : public QObject
{
    Q_OBJECT
public:
    ArchiveModelPrivate(ArchiveModel *q, QObject *parent = nullptr);

    void setStatus(ArchiveModel::Status s, ArchiveModel::ErrorState e);
    void updateDirectory();
    void calculateSpaceRequirement(const KArchiveDirectory *dir, const QString &path);
    void calculateSpaceRequirement();

    bool open(const QString &file);

    void scheduleExtract(const QString &entryName, const QString &targetPath, ExtractionMode mode);

    ArchiveModel *q;
    ArchiveModel::Status status;
    ArchiveModel::ErrorState errorState;
    qint64 requiredSpace;
    QString path;
    QString errorString;
    bool populated;
    bool extracting;
    bool active;
    bool autoRename;
    ArchiveInfo archiveInfo;
    QList<const KArchiveEntry*> entryList;
    QMimeDatabase mimeDatabase;
    QFutureWatcher<FileExtractionResult> *extractionWatcher;

    QMutex extractionMutex;

    KArchive* archive;
    const KArchiveDirectory *currentDirectory;

    friend class ArchiveModel;

public slots:
    Sailfish::FileExtractionResult doExtractFile(const QString &entryName, const QString &targetPath);
    Sailfish::FileExtractionResult doExtractAllFiles(const QString &targetPath);
};

}
#endif // SAILFISH_ARCHIVE_LISTMODEL_P_H
