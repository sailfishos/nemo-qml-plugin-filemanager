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

#include "fileworker.h"
#include <QDateTime>
#include <QQmlInfo>

FileWorker::FileWorker(QObject *parent) :
    QThread(parent),
    m_mode(FileEngine::IdleMode),
    m_cancelled(KeepRunning)
{
    connect(this, &FileWorker::finished, this, &FileWorker::handleFinished);
}

FileWorker::~FileWorker()
{
}

void FileWorker::handleFinished()
{
    setMode(FileEngine::IdleMode);
}

FileEngine::Mode FileWorker::mode() const
{
    return m_mode;
}

void FileWorker::setMode(FileEngine::Mode mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged();
    }
}

void FileWorker::startDeleteFiles(QStringList fileNames)
{
    if (isRunning()) {
        emit error(FileEngine::ErrorOperationInProgress, "");
        return;
    }

    if (!validateFileNames(fileNames))
        return;

    setMode(FileEngine::DeleteMode);
    m_fileNames = fileNames;
    m_cancelled.storeRelease(KeepRunning);
    start();
}

void FileWorker::startCopyFiles(QStringList fileNames, QString destDirectory)
{
    if (isRunning()) {
        emit error(FileEngine::ErrorOperationInProgress, "");
        return;
    }

    if (!validateFileNames(fileNames))
        return;

    setMode(FileEngine::CopyMode);

    m_fileNames = fileNames;
    m_destDirectory = destDirectory;
    m_cancelled.storeRelease(KeepRunning);
    start();
}

void FileWorker::startMoveFiles(QStringList fileNames, QString destDirectory)
{
    if (isRunning()) {
        emit error(FileEngine::ErrorOperationInProgress, "");
        return;
    }

    if (!validateFileNames(fileNames))
        return;

    setMode(FileEngine::MoveMode);

    m_fileNames = fileNames;
    m_destDirectory = destDirectory;
    m_cancelled.storeRelease(KeepRunning);
    start();
}

void FileWorker::cancel()
{
    m_cancelled.storeRelease(Cancelled);
}

bool FileWorker::mkdir(QString path, QString name)
{
    QDir dir(path);
    return dir.mkdir(name);
}

bool FileWorker::rename(QString oldPath, QString newPath)
{
    QFile file(oldPath);
    return file.rename(newPath);
}

bool FileWorker::setPermissions(QString path, QFileDevice::Permissions p)
{
    QFile file(path);
    return file.setPermissions(p);
}

void FileWorker::run()
{
    switch (m_mode) {
    case FileEngine::DeleteMode:
        deleteFiles();
        break;

    case FileEngine::MoveMode:
    case FileEngine::CopyMode:
        copyOrMoveFiles();
        break;
    case FileEngine::IdleMode:
        qmlInfo(this) << "FileWorker run in IdleMode, should not happen";
        break;
    }
}

bool FileWorker::validateFileNames(const QStringList &fileNames)
{
    // basic validity check
    foreach (QString fileName, fileNames) {
        if (fileName.isEmpty()) {
            return false;
        }
    }
    return true;
}

bool FileWorker::deleteFile(QString fileName)
{
    QFileInfo info(fileName);
    if (!info.exists() && !info.isSymLink()) {
        return false;
    }
    if (info.isDir() && info.isSymLink()) {
        // only delete the link and do not remove recursively subfolders
        QFile file(info.absoluteFilePath());
        return file.remove();
    } else if (info.isDir()) {
        // this should be custom function to get better error reporting
        return QDir(info.absoluteFilePath()).removeRecursively();
    } else {
        QFile file(info.absoluteFilePath());
        return file.remove();
    }
    return true;
}

void FileWorker::deleteFiles()
{
    int fileIndex = 0;

    foreach (QString fileName, m_fileNames) {

        // stop if cancelled
        if (m_cancelled.loadAcquire() == Cancelled) {
            emit cancelled();
            return;
        }

        // delete file and stop if errors
        bool success = deleteFile(fileName);
        if (!success) {
            emit error(FileEngine::ErrorDeleteFailed, fileName);
            return;
        }
        emit fileDeleted(fileName);

        fileIndex++;
    }

    emit done();
}

void FileWorker::copyOrMoveFiles()
{
    int fileIndex = 0;

    QDir dest(m_destDirectory);
    foreach (QString fileName, m_fileNames) {

        // stop if cancelled
        if (m_cancelled.loadAcquire() == Cancelled) {
            emit cancelled();
            return;
        }

        QFileInfo fileInfo(fileName);
        QString newName = dest.absoluteFilePath(fileInfo.fileName());

        // move or copy and stop if errors
        QFile file(fileName);
        if (m_mode == FileEngine::MoveMode) {
            if (fileInfo.isSymLink()) {
                // move symlink by creating a new link and deleting the old one
                QFile targetFile(fileInfo.symLinkTarget());
                if (!targetFile.link(newName)) {
                    emit error(FileEngine::ErrorMoveFailed, fileName);
                    return;
                }
                if (!file.remove()) {
                    emit error(FileEngine::ErrorMoveFailed, fileName);
                    return;
                }

            } else if (!file.rename(newName)) {
                emit error(FileEngine::ErrorMoveFailed, fileName);
                return;
            }

        } else { // CopyMode
            if (fileInfo.isDir()) {
                bool ok = copyDirRecursively(fileName, newName);
                if (!ok) {
                    emit error(FileEngine::ErrorFolderCopyFailed, fileName);
                    return;
                }
            } else {
                if (!copyOverwrite(fileName, newName)) {
                    emit error(FileEngine::ErrorCopyFailed, fileName);
                    return;
                }
            }
        }

        fileIndex++;
    }

    emit done();
}

bool FileWorker::copyDirRecursively(QString srcDirectory, QString destDirectory)
{
    QFileInfo srcInfo(srcDirectory);
    if (srcInfo.isSymLink()) {
        // copy dir symlink by creating a new link
        QFile targetFile(srcInfo.symLinkTarget());
        if (!targetFile.link(destDirectory)) {
            return false;
        }
    }

    QDir srcDir(srcDirectory);
    if (!srcDir.exists()) {
        return false;
    }

    QDir destDir(destDirectory);
    if (!destDir.exists()) {
        QDir d(destDir);
        d.cdUp();
        if (!d.mkdir(destDir.dirName())) {
            return false;
        }
    }

    // copy files
    QStringList names = srcDir.entryList(QDir::Files);
    for (int i = 0 ; i < names.count() ; ++i) {
        QString fileName = names.at(i);

        // stop if cancelled
        if (m_cancelled.loadAcquire() == Cancelled) {
            emit cancelled();
            return false;
        }

        QString spath = srcDir.absoluteFilePath(fileName);
        QString dpath = destDir.absoluteFilePath(fileName);
        if (!copyOverwrite(spath, dpath)) {
            return false;
        }
    }

    // copy dirs
    names = srcDir.entryList(QDir::NoDotAndDotDot | QDir::AllDirs);
    for (int i = 0 ; i < names.count() ; ++i) {
        QString fileName = names.at(i);

        // stop if cancelled
        if (m_cancelled.loadAcquire() == Cancelled) {
            emit cancelled();
            return false;
        }

        QString spath = srcDir.absoluteFilePath(fileName);
        QString dpath = destDir.absoluteFilePath(fileName);
        if (!copyDirRecursively(spath, dpath)) {
            return false;
        }
    }

    return true;
}

bool FileWorker::copyOverwrite(QString src, QString dest)
{
    // delete destination if it exists
    QFile dfile(dest);
    if (dfile.exists() && !dfile.remove()) {
        return false;
    }

    QFileInfo fileInfo(src);
    if (fileInfo.isSymLink()) {
        // copy symlink by creating a new link
        QFile targetFile(fileInfo.symLinkTarget());
        return targetFile.link(dest);
    }

    // normal file copy
    QFile sfile(src);
    return sfile.copy(dest);
}

