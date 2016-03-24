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

#include "fileengine.h"
#include "fileworker.h"
#include "statfileinfo.h"
#include <QDateTime>
#include <QDebug>
#include <QQmlInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <unistd.h>

Q_GLOBAL_STATIC(FileEngine, engine);


FileEngine::FileEngine(QObject *parent) :
    QObject(parent),
    m_clipboardContainsCopy(false)
{
    m_fileWorker = new FileWorker(this);


    // pass worker end signals to QML
    connect(m_fileWorker, &FileWorker::done, this, &FileEngine::workerDone);
    connect(m_fileWorker, &FileWorker::cancelled, this, &FileEngine::cancelled);
    connect(m_fileWorker, &FileWorker::error, this, &FileEngine::error);

    connect(m_fileWorker, &FileWorker::started, this, &FileEngine::busyChanged);
    connect(m_fileWorker, &FileWorker::finished, this, &FileEngine::busyChanged);

    connect(m_fileWorker, &FileWorker::fileDeleted, this, &FileEngine::fileDeleted);
}

FileEngine::~FileEngine()
{
    // is this the way to force stop the worker thread?
    m_fileWorker->cancel(); // stop possibly running background thread
    m_fileWorker->wait();   // wait until thread stops
}

FileEngine *FileEngine::instance()
{
    return engine();
}

bool FileEngine::busy() const
{
    return m_fileWorker->isRunning();
}

void FileEngine::deleteFiles(QStringList fileNames)
{
    m_fileWorker->startDeleteFiles(fileNames);
}

void FileEngine::cutFiles(QStringList fileNames)
{
    m_clipboardFiles = fileNames;
    m_clipboardContainsCopy = false;
    emit clipboardCountChanged();
    emit clipboardContainsCopyChanged();
}

void FileEngine::copyFiles(QStringList fileNames)
{
    // don't copy special files (chr/blk/fifo/sock)
    QMutableStringListIterator i(fileNames);
    while (i.hasNext()) {
        QString fileName = i.next();
        StatFileInfo info(fileName);
        if (info.isSystem())
            i.remove();
    }

    if (!fileNames.isEmpty()) {
        m_clipboardFiles = fileNames;
        m_clipboardContainsCopy = true;
        emit clipboardCountChanged();
        emit clipboardContainsCopyChanged();
    }
}

void FileEngine::pasteFiles(QString destDirectory)
{
    if (m_clipboardFiles.isEmpty()) {
        qmlInfo(this) << "Paste called with empty clipboard.";
        return;
    }

    QStringList files = m_clipboardFiles;

    QDir dest(destDirectory);
    if (!dest.exists()) {
        qmlInfo(this) << "Paste destination doesn't exists";
        return;
    }

    foreach (QString fileName, files) {
        QFileInfo fileInfo(fileName);
        QString newName = dest.absoluteFilePath(fileInfo.fileName());

        // source and dest fileNames are the same?
        if (fileName == newName) {
            qmlInfo(this) << "Paste can't overwrite itself";
            return;
        }

        // dest is under source? (directory)
        if (newName.startsWith(fileName)) {
            emit error(ErrorCannotCopyIntoItself, fileName);
            return;
        }
    }

    m_clipboardFiles.clear();
    emit clipboardCountChanged();

    if (m_clipboardContainsCopy) {
        m_fileWorker->startCopyFiles(files, destDirectory);
        return;
    }

    m_fileWorker->startMoveFiles(files, destDirectory);
}

void FileEngine::cancel()
{
    m_fileWorker->cancel();
}

bool FileEngine::exists(QString fileName)
{
    if (fileName.isEmpty()) {
        return false;
    }

    return QFile::exists(fileName);
}

bool FileEngine::mkdir(QString path, QString name)
{
    QDir dir(path);

    if (!dir.mkdir(name)) {
        emit error(ErrorFolderCreationFailed, name);
        return false;
    }

    return true;
}

bool FileEngine::rename(QString fullOldFileName, QString newName)
{
    QFile file(fullOldFileName);
    QFileInfo fileInfo(fullOldFileName);
    QDir dir = fileInfo.absoluteDir();
    QString fullNewFileName = dir.absoluteFilePath(newName);

    if (!file.rename(fullNewFileName)) {
        emit error(ErrorRenameFailed, fileInfo.fileName());
        return false;
    }
    return true;
}

bool FileEngine::chmod(QString path,
                      bool ownerRead, bool ownerWrite, bool ownerExecute,
                      bool groupRead, bool groupWrite, bool groupExecute,
                      bool othersRead, bool othersWrite, bool othersExecute)
{
    QFile file(path);
    QFileDevice::Permissions p;
    if (ownerRead) p |= QFileDevice::ReadOwner;
    if (ownerWrite) p |= QFileDevice::WriteOwner;
    if (ownerExecute) p |= QFileDevice::ExeOwner;
    if (groupRead) p |= QFileDevice::ReadGroup;
    if (groupWrite) p |= QFileDevice::WriteGroup;
    if (groupExecute) p |= QFileDevice::ExeGroup;
    if (othersRead) p |= QFileDevice::ReadOther;
    if (othersWrite) p |= QFileDevice::WriteOther;
    if (othersExecute) p |= QFileDevice::ExeOther;
    if (!file.setPermissions(p)) {
        emit error(ErrorChmodFailed, path);
        return false;
    }
    return true;
}