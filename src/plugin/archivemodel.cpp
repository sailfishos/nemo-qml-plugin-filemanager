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

#include "archivemodel.h"
#include "archivemodel_p.h"
#include "archiveinfo.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QPair>
#include <QQmlInfo>
#include <QStandardPaths>
#include <QtConcurrent>

// From KArchive
#include <k7zip.h>
#include <kzip.h>
#include <ktar.h>

Q_LOGGING_CATEGORY(lcArchiveLog, "org.sailfishos.archivelistmodel", QtWarningMsg)

bool compare(const KArchiveEntry *i, const KArchiveEntry *j) {
    if (i->isDirectory() && j->isFile()) {
        return true;
    } else if (i->isFile() && j->isDirectory()) {
        return false;
    } else {
        return i->name().compare(j->name(), Qt::CaseInsensitive) < 0;
    }

    return (i<j);
}

namespace {

enum {
    FileNameRole = Qt::UserRole + 1,
    MimeTypeRole,
    SizeRole,
    CreatedRole,
    IsDirRole,
    IsLinkRole,
    ExtractedRole,
    ExtractedTargetPathRole,
    SymLinkTargetRole,
    IsSelectedRole,
    ExtensionRole,
    AbsolutePathRole,
    BaseNameRole
};

}

namespace Sailfish {

ArchiveModelPrivate::ArchiveModelPrivate(ArchiveModel *q, QObject *parent)
    : QObject(parent)
    , q(q)
    , status(ArchiveModel::Null)
    , errorState(ArchiveModel::NoError)
    , requiredSpace(0)
    , path(QDir::separator())
    , populated(false)
    , extracting(false)
    , active(false)
    , autoRename(false)
    , extractionWatcher(nullptr)
    , archive(nullptr)
    , currentDirectory(nullptr)
{
    connect(&archiveInfo, &ArchiveInfo::fileNameChanged, q, &ArchiveModel::fileNameChanged);
    connect(&archiveInfo, &ArchiveInfo::baseNameChanged, q, &ArchiveModel::baseNameChanged);
    connect(&archiveInfo, &ArchiveInfo::completeSuffixChanged, q, &ArchiveModel::completeSuffixChanged);
}

void ArchiveModelPrivate::setStatus(ArchiveModel::Status s, ArchiveModel::ErrorState e)
{
    switch (e) {
    case ArchiveModel::NoError:
        errorString.clear();
        break;
    case ArchiveModel::ErrorArchiveNotFound:
        errorString = QString("Archive not found: %1").arg(archiveInfo.file());
        break;
    case ArchiveModel::ErrorArchiveOpenFailed:
        errorString = QString("Failed to open archive: %1. Check read permissions.").arg(archiveInfo.file());
        break;
    case ArchiveModel::ErrorInvalidArchivePath:
        errorString = QString("Invalid archive path. Is archive broken?");
        break;
    case ArchiveModel::ErrorInvalidArchiveEntry:
        errorString = QString("Invalid archive entry. Is archive broken?");
        break;
    case ArchiveModel::ErrorUnsupportedArchiveFormat:
        errorString = QString("Unsupported archive %1").arg(archiveInfo.fileName());
        break;
    default:
        break;
    }

    if (errorState != e) {
        errorState = e;
        emit q->errorStateChanged();
    }

    if (status != s) {
        status = s;
        emit q->statusChanged();
    }
}

void ArchiveModelPrivate::updateDirectory()
{
    if (!currentDirectory) {
        qCWarning(lcArchiveLog) << "No active directory. Source of bug!";
        setStatus(ArchiveModel::Error, ArchiveModel::ErrorArchiveFileNoSet);
        return;
    }

    const KArchiveEntry *entry = currentDirectory->entry(path);
    if (!entry || !entry->isDirectory()) {
        setStatus(ArchiveModel::Error, ArchiveModel::ErrorInvalidArchivePath);
        qCWarning(lcArchiveLog) << "Entry" << path << "doesn't exist in the archive folder. Source of bug or a broken archive!";
        return;
    }

    setStatus(ArchiveModel::Loading, ArchiveModel::NoError);
    currentDirectory = static_cast<const KArchiveDirectory *>(entry);

    entryList.clear();
    QStringList list = currentDirectory->entries();

    for (const QString &entryStr : list) {
        const KArchiveEntry *entry = currentDirectory->entry(entryStr);
        if (!entry) {
            qCWarning(lcArchiveLog) << "Entry" << entryStr << "doesn't exist in the archive folder. Source of bug!";
            setStatus(ArchiveModel::Error, ArchiveModel::ErrorInvalidArchiveEntry);
            break;
        }

        entryList.append(entry);
    }

    std::sort(entryList.begin(), entryList.end(), compare);
    setStatus(ArchiveModel::Ready, ArchiveModel::NoError);
}

void ArchiveModelPrivate::calculateSpaceRequirement(const KArchiveDirectory *dir, const QString &path)
{
    QStringList entries = dir->entries();
    entries.sort();

    for (const QString &entryName : entries) {
        const KArchiveEntry *entry = dir->entry(entryName);
        if (entry->isFile()) {
            requiredSpace += static_cast<const KArchiveFile *>(entry)->size();
        } else if (entry->isDirectory()) {
            calculateSpaceRequirement((KArchiveDirectory *)entry, path + entryName + QDir::separator());
        }
    }
}

void ArchiveModelPrivate::calculateSpaceRequirement()
{
    qint64 reqSpace = requiredSpace;
    const KArchiveDirectory *dir = archive->directory();
    calculateSpaceRequirement(dir, QLatin1String(""));
    if (reqSpace != requiredSpace) {
        emit q->requiredSpaceChanged();
    }
}

bool ArchiveModelPrivate::open(const QString &file)
{
    if (!ArchiveInfo::exists(file)) {
        setStatus(ArchiveModel::Error, ArchiveModel::ErrorArchiveNotFound);
        qCWarning(lcArchiveLog) << "Archive not found: " << file;
        return false;
    }

    archiveInfo.setFile(file);
    delete archive;

    if (!archiveInfo.supported()) {
        setStatus(ArchiveModel::Error, ArchiveModel::ErrorUnsupportedArchiveFormat);
        return false;
    }

    if (archiveInfo.format() == ArchiveInfo::Zip) {
        archive = new KZip(archiveInfo.file());
    } else if (archiveInfo.format() == ArchiveInfo::SevenZip) {
        archive = new K7Zip(archiveInfo.file());
    } else {
        archive = new KTar(archiveInfo.file());
    }

    bool opened = archive->open(QIODevice::ReadOnly);
    if (!opened) {
        setStatus(ArchiveModel::Error, ArchiveModel::ErrorArchiveOpenFailed);
        qCWarning(lcArchiveLog) << "Failed to open archive." << archive;
        return false;
    }

    return opened;
}

void ArchiveModelPrivate::scheduleExtract(const QString &entryName, const QString &targetPath, ExtractionMode mode)
{
    extractionWatcher = new QFutureWatcher<FileExtractionResult>();
    connect(extractionWatcher, &QFutureWatcher<FileExtractionResult>::finished, this, [this, mode]() {
        FileExtractionResult result = extractionWatcher->result();
        ArchiveModel::ErrorState state = result.first;
        ExtractionInfo extractionInfo = result.second;

        if (state == ArchiveModel::NoError) {
            setStatus(ArchiveModel::Ready, state);
        } else {
            setStatus(ArchiveModel::Error, state);
        }

        extracting = false;
        emit q->extractingChanged();

        if (state == ArchiveModel::NoError) {
            QString absolutePath = extractionInfo.absolutePath;
            QFileInfo entryInfo(absolutePath);
            bool isDir = entryInfo.isDir();
            QStringList extractedFiles;
            if (mode == ExtractionMode::SingleFile && !isDir) {
                extractedFiles << absolutePath;
            } else {
                QDir d(absolutePath, QString(), QDir::Name | QDir::DirsFirst, QDir::NoDotAndDotDot | QDir::AllEntries);
                QDirIterator dirIterator(d, QDirIterator::Subdirectories);
                while (dirIterator.hasNext()) {
                    QString f = dirIterator.next();
                    extractedFiles << f;
                }
            }

            QDir entryDir(absolutePath);
            QString name;
            if (extractedFiles.count() == 1) {
                entryInfo.setFile(extractedFiles.at(0));
                name = entryInfo.fileName();
            } else {
                name = entryDir.dirName();
            }

            if (mode == ExtractionMode::SingleFile) {
                int i = findIndex(extractionInfo.entry);
                if (i >= 0) {
                    if (!extractedEntries.contains(extractionInfo.entry)) {
                        extractedEntries.insert(extractionInfo.entry, extractionInfo);
                        QModelIndex index = q->index(i, 0);
                        emit q->dataChanged(index, index, QVector<int>() << ExtractedRole << ExtractedTargetPathRole);
                    } else {
                        qCWarning(lcArchiveLog) << extractionInfo.entry << "is already extracted.";
                    }
                } else {
                    qCWarning(lcArchiveLog) << extractionInfo.entry << "is not part of this archive directory. Source of bug!";
                }
            }

            // When extracting an archive entryInfo is a directory. Thus, checking that we're extracting a single file
            // from an archive that is a directory.
            emit q->filesExtracted(entryDir.absolutePath(), (mode == ExtractionMode::SingleFile && entryInfo.isDir()),
                                   name, extractedFiles);
        }
        extractionWatcher->deleteLater();
        extractionWatcher = nullptr;
    });

    // Make sure that target path ends with "/"
    QString path = targetPath;
    if (!path.endsWith(QDir::separator())) {
        path.append(QDir::separator());
    }

    setStatus(ArchiveModel::Extracting, ArchiveModel::NoError);
    extracting = true;
    emit q->extractingChanged();

    if (mode == ExtractionMode::SingleFile) {
        extractionWatcher->setFuture(QtConcurrent::run(this, &ArchiveModelPrivate::doExtractFile, entryName, path));
    } else {
        extractionWatcher->setFuture(QtConcurrent::run(this, &ArchiveModelPrivate::doExtractAllFiles, path));
    }
}

int ArchiveModelPrivate::findIndex(const QString &entryName)
{
    for (int i = 0; i < entryList.count(); ++i) {
        if (entryList.at(i)->name() == entryName)
            return i;
    }
    return -1;
}

FileExtractionResult ArchiveModelPrivate::doExtractFile(const QString &entryName, const QString &targetPath)
{
    QMutexLocker lock(&extractionMutex);
    FileExtractionResult result;

    const KArchiveEntry *entry = currentDirectory->entry(entryName);
    if (!entry) {
        qCWarning(lcArchiveLog) << "Entry" << entryName << "doesn't exist in archive path"
                                << path << ", cannot extract. A source of bug!";
        result.first = ArchiveModel::ErrorInvalidArchiveEntry;
        return result;
    }

    qCDebug(lcArchiveLog) << "Extracting a file:" << targetPath;

    bool extracted = false;
    QString out;
    if (entry->isDirectory()) {
        const KArchiveDirectory *dir = static_cast<const KArchiveDirectory *>(entry);
        extracted = dir->copyTo(targetPath, true, autoRename, &out);
        qCDebug(lcArchiveLog) << "Extraction output directory:" << out;
    } else {
        const KArchiveFile *file = static_cast<const KArchiveFile *>(entry);
        extracted = file->copyTo(targetPath, autoRename, &out);
        qCDebug(lcArchiveLog) << "Extracted out file:" << out;
    }

    if (extracted) {
        result.first = ArchiveModel::NoError;
        result.second = ExtractionInfo(entryName, out);
    } else {
        result.first = ArchiveModel::ErrorArchiveExtractFailed;
    }

    return result;
}

FileExtractionResult ArchiveModelPrivate::doExtractAllFiles(const QString &targetPath)
{
    QMutexLocker lock(&extractionMutex);
    FileExtractionResult result;

    const KArchiveDirectory *dir = archive->directory();
    if (!dir) {
        qCWarning(lcArchiveLog) << "Archive root directory doesn't exist, cannot extract. A source of bug or a broken archive!";
        result.first = ArchiveModel::ErrorInvalidArchivePath;
        return result;
    }

    QString out;
    if (dir->copyTo(targetPath, true, true, &out)) {
        result.first = ArchiveModel::NoError;
        result.second = ExtractionInfo("", out);
        qCDebug(lcArchiveLog) << "Extracted archive to:" << out;
    } else {
        result.first = ArchiveModel::ErrorArchiveExtractFailed;
    }

    return result;
}

ArchiveModel::ArchiveModel(QObject *parent)
    : QAbstractListModel(parent)
    , d(new ArchiveModelPrivate(this))
{
    connect(this, &ArchiveModel::populatedChanged, this, [this]() {
        if (d->path != QDir::separator()) {
            beginResetModel();
            d->updateDirectory();
            // Sligtly pessimistic as single file can be extracted as well.
            d->calculateSpaceRequirement();
            endResetModel();
        }
    });
}

ArchiveModel::~ArchiveModel()
{
    delete d;
    d = nullptr;
}

int ArchiveModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return d->entryList.size();
}

QVariant ArchiveModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= d->entryList.size())
        return QVariant();

    const KArchiveEntry *entry = d->entryList.at(index.row());
    switch (role) {

    case Qt::DisplayRole:
    case FileNameRole:
        return entry->name();

    case MimeTypeRole: {
        QString mimeType;
        // Return an empty mime type for a directory.
        if (entry->isFile()) {
            // Do not open, do best effort based on file name.
            QList<QMimeType> mimeTypes = d->mimeDatabase.mimeTypesForFileName(entry->name());
            if (!mimeTypes.isEmpty()) {
                mimeType = mimeTypes.at(0).name();
            }
        }

        return mimeType;
    }

    case SizeRole:
        if (entry->isFile()) {
            return static_cast<const KArchiveFile *>(entry)->size();
        }
        return 0;

    case ExtractedRole:
        return d->extractedEntries.contains(entry->name());

    case ExtractedTargetPathRole:
        if (d->extractedEntries.contains(entry->name())) {
            const ExtractionInfo &extrationInfo = d->extractedEntries.value(entry->name());
            return extrationInfo.absolutePath;
        }
        return QString();

    case CreatedRole:
        return entry->date();

    case IsDirRole:
        return entry->isDirectory();

    case IsLinkRole:
        return !entry->symLinkTarget().isEmpty();

    case SymLinkTargetRole:
        return entry->symLinkTarget();

    case IsSelectedRole:
        return false;

    case ExtensionRole: {
        QFileInfo info(entry->name());
        return info.completeBaseName().isEmpty() ? QString() : info.suffix();
    }

    case AbsolutePathRole:
        return QString();

    case BaseNameRole: {
        QFileInfo info(entry->name());
        QString baseName(info.completeBaseName());
        return baseName.isEmpty() ? info.fileName() : baseName;
    }

    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ArchiveModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles.insert(FileNameRole, QByteArray("fileName"));
    roles.insert(MimeTypeRole, QByteArray("mimeType"));
    roles.insert(SizeRole, QByteArray("size"));
    roles.insert(CreatedRole, QByteArray("created"));
    roles.insert(ExtractedRole, QByteArray("extracted"));
    roles.insert(ExtractedTargetPathRole, QByteArray("extractedTargetPath"));
    roles.insert(IsDirRole, QByteArray("isDir"));
    roles.insert(IsLinkRole, QByteArray("isLink"));
    roles.insert(SymLinkTargetRole, QByteArray("symLinkTarget"));
    roles.insert(IsSelectedRole, QByteArray("isSelected"));
    roles.insert(ExtensionRole, QByteArray("extension"));
    roles.insert(AbsolutePathRole, QByteArray("absolutePath"));
    roles.insert(BaseNameRole, QByteArray("baseName"));
    return roles;
}

QString ArchiveModel::path() const
{
    return d->path;
}

void ArchiveModel::setPath(const QString &path)
{
    if (d->extracting) {
        qmlInfo(this) << "Extracting already. Wait for extraction to complete to set path.";
        return;
    }

    if (d->path != path) {
        QMutexLocker lock(&d->extractionMutex);
        d->path = path;
        if (d->populated) {
            beginResetModel();
            d->updateDirectory();
            endResetModel();
        }

        emit pathChanged();
    }
}

ArchiveModel::Status ArchiveModel::status() const
{
    return d->status;
}

ArchiveModel::ErrorState ArchiveModel::errorState()
{
    return d->errorState;
}

bool ArchiveModel::populated() const
{
    return d->populated;
}

bool ArchiveModel::extracting() const
{
    return d->extracting;
}

int ArchiveModel::count() const
{
    return rowCount();
}

bool ArchiveModel::active() const
{
    return  d->active;
}

void ArchiveModel::setActive(bool active)
{
    if (d->active != active) {
        d->active = active;
        emit activeChanged();
    }
}

bool ArchiveModel::autoRename() const
{
    return d->autoRename;
}

void ArchiveModel::setAutoRename(bool autoRename)
{
    if (d->autoRename != autoRename) {
        d->autoRename = autoRename;
        emit autoRenameChanged();
    }
}

QString ArchiveModel::archiveFile() const
{
    return d->archiveInfo.file();
}

void ArchiveModel::setArchiveFile(const QString &archiveFile)
{
    QString localFile = archiveFile;
    QUrl url(archiveFile);
    if (url.isLocalFile()) {
        localFile = url.toLocalFile();
    }

    if (this->archiveFile() != localFile) {
        QMutexLocker lock(&d->extractionMutex);
        if (!d->open(localFile)) {
            return;
        }

        d->currentDirectory = d->archive->directory();

        beginResetModel();
        d->updateDirectory();
        // Sligtly pessimistic as single file can be extracted as well.
        d->calculateSpaceRequirement();
        endResetModel();

        qCDebug(lcArchiveLog) << "Archive:" << d->archiveInfo.file() << "path:" << d->path;

        d->populated = true;

        emit populatedChanged();
        emit archiveFileChanged();
    }
}

QString ArchiveModel::fileName() const
{
    return d->archiveInfo.fileName();
}

QString ArchiveModel::baseName() const
{
    return d->archiveInfo.baseName();
}

QString ArchiveModel::completeSuffix() const
{
    return d->archiveInfo.completeSuffix();
}

qint64 ArchiveModel::requiredSpace() const
{
    return d->requiredSpace;
}

QString ArchiveModel::appendPath(const QString &pathName) const
{
    QMutexLocker lock(&d->extractionMutex);
    if (!d->currentDirectory) {
        qCWarning(lcArchiveLog) << "No valid directory, source of bug!";
        d->setStatus(Error, ErrorArchiveFileNoSet);
        return QString();
    }

    QString targetPath = d->path;

    if (!targetPath.endsWith(QDir::separator())) {
        targetPath.append(QDir::separator());
    }

    targetPath.append(pathName);


    const KArchiveEntry *entry = d->currentDirectory->entry(pathName);
    if (!entry) {
        d->setStatus(Error, ErrorInvalidArchiveEntry);
        qCWarning(lcArchiveLog) << "Entry is missing from the directory, cannot append path. Source of bug!";
        return QString();
    } else if (!entry->isDirectory()) {
        d->setStatus(Error, ErrorInvalidArchivePath);
        qCWarning(lcArchiveLog) << "Entry path" << pathName << "doesn't exist in the archive, cannot append path. Source of bug!";
        return QString();
    }

    return targetPath;
}

QString ArchiveModel::errorString() const
{
    return d->errorString;
}

bool ArchiveModel::extractAllFiles(const QString &targetPath)
{
    if (d->extracting) {
        qmlInfo(this) << "Extracting already. Wait for extraction to complete.";
        d->setStatus(Extracting, ErrorExtractingInProgress);
        return false;
    }

    if (!d->archive) {
        qmlInfo(this) << "Archive file is not set. Set archiveFile property before extracting.";
        d->setStatus(Error, ErrorArchiveFileNoSet);
        return false;
    }

    d->scheduleExtract(QString(), targetPath, ExtractionMode::Archive);
    return true;
}

bool ArchiveModel::extractFile(const QString &entryName, const QString &targetPath)
{
    if (d->extracting) {
        qmlInfo(this) << "Extracting already. Wait for extraction to complete.";
        d->setStatus(Extracting, ErrorExtractingInProgress);
        return false;
    }

    if (!d->currentDirectory) {
        qCWarning(lcArchiveLog) << "No valid directory, source of bug. Remember to set archiveFile.";

        if (!d->archive) {
            qmlInfo(this) << "Archive file is not set. Set archiveFile property before extracting.";
            d->setStatus(Error, ErrorArchiveFileNoSet);
        }
        return false;
    }

    d->scheduleExtract(entryName, targetPath, ExtractionMode::SingleFile);
    return true;
}

bool ArchiveModel::cleanExtractedEntry(const QString &entry)
{
    if (d->extracting) {
        qmlInfo(this) << "Extracting, wait for extraction to complete.";
        d->setStatus(Extracting, ErrorExtractingInProgress);
        return false;
    }

    int entryIndex = d->findIndex(entry);
    if (d->extractedEntries.contains(entry) && entryIndex >= 0) {
        d->extractedEntries.remove(entry);
        QModelIndex index = this->index(entryIndex, 0);
        emit dataChanged(index, index, QVector<int>() << ExtractedRole << ExtractedTargetPathRole);
    } else {
        qmlInfo(this) << "Entry:" << entry << "is not extracted.";
    }

    return true;
}

}
