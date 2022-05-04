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
#include "fileoperations.h"
#include "diskusage.h"

#include <QQmlInfo>
#include <QCoreApplication>

FileWorker::FileWorker(QObject *parent) :
    QThread(parent),
    m_proxy(QString("org.nemomobile.FileOperations"), QString("/"), QDBusConnection::sessionBus()),
    m_operation(0),
    m_mode(FileEngine::IdleMode),
    m_cancelled(KeepRunning)
{
    connect(this, &FileWorker::finished, this, &FileWorker::handleFinished);

    connect(&m_proxy, &FileOperationsProxy::Failed, this, &FileWorker::fileOperationFailed);
    connect(&m_proxy, &FileOperationsProxy::Succeeded, this, &FileWorker::fileOperationSucceeded);
    connect(&m_proxy, &FileOperationsProxy::Finished, this, &FileWorker::operationFinished);
}

FileWorker::~FileWorker()
{
}

void FileWorker::handleFinished()
{
    m_operation = 0;
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

void FileWorker::startDeleteFiles(QStringList fileNames, bool nonprivileged)
{
    if (operationInProgress()) {
        emit error(FileEngine::ErrorOperationInProgress, "");
        return;
    }

    if (!validateFileNames(fileNames)) {
        emit error(FileEngine::ErrorInvalidRequest, "");
        return;
    }

    setMode(FileEngine::DeleteMode);
    m_fileNames = fileNames;
    m_cancelled.storeRelease(KeepRunning);
    startOperation(nonprivileged);
}

void FileWorker::startCopyFiles(QStringList fileNames, QString destDirectory, bool nonprivileged)
{
    if (operationInProgress()) {
        emit error(FileEngine::ErrorOperationInProgress, "");
        return;
    }

    if (!validateFileNames(fileNames)) {
        emit error(FileEngine::ErrorInvalidRequest, "");
        return;
    }

    setMode(FileEngine::CopyMode);

    m_fileNames = fileNames;
    m_destDirectory = destDirectory;
    m_cancelled.storeRelease(KeepRunning);
    startOperation(nonprivileged);
}

void FileWorker::startMoveFiles(QStringList fileNames, QString destDirectory, bool nonprivileged)
{
    if (operationInProgress()) {
        emit error(FileEngine::ErrorOperationInProgress, "");
        return;
    }

    if (!validateFileNames(fileNames)) {
        emit error(FileEngine::ErrorInvalidRequest, "");
        return;
    }

    setMode(FileEngine::MoveMode);

    m_fileNames = fileNames;
    m_destDirectory = destDirectory;
    m_cancelled.storeRelease(KeepRunning);
    startOperation(nonprivileged);
}

void FileWorker::cancel()
{
    if (m_operation != 0) {
        m_proxy.Cancel(m_operation);
    } else if (isRunning()) {
        m_cancelled.storeRelease(Cancelled);
    }
}

bool FileWorker::mkdir(QString path, QString name, bool nonprivileged)
{
    if (!nonprivileged) {
        return FileOperations::createDirectory(name, path) == FileEngine::NoError;
    }

    QDBusPendingReply<uint> reply = m_proxy.Mkdir(name, path);
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(diskUsage) << "Error waiting for file operation reply";
        return false;
    }

    return waitForOperation(reply.value());
}

bool FileWorker::rename(QString oldPath, QString newPath, bool nonprivileged)
{
    if (!nonprivileged) {
        return FileOperations::renameFile(oldPath, newPath) == FileEngine::NoError;
    }

    QDBusPendingReply<uint> reply = m_proxy.Rename(oldPath, newPath);
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(diskUsage) << "Error waiting for file operation reply";
        return false;
    }

    return waitForOperation(reply.value());
}

bool FileWorker::setPermissions(QString path, QFileDevice::Permissions p, bool nonprivileged)
{
    if (!nonprivileged) {
        return FileOperations::chmodFile(path, p) == FileEngine::NoError;
    }

    QDBusPendingReply<uint> reply = m_proxy.SetPermissions(path, static_cast<unsigned>(p));
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(diskUsage) << "Error waiting for file operation reply";
        return false;
    }

    return waitForOperation(reply.value());
}

bool FileWorker::operationInProgress()
{
    return isRunning() || m_operation != 0;
}

void FileWorker::startOperation(bool nonprivileged)
{
    if (!nonprivileged) {
        // Run threaded
        start();
        return;
    }

    // Delegate this operation to the nonprivileged daemon
    QDBusPendingReply<uint> reply;

    switch (m_mode) {
    case FileEngine::DeleteMode:
        reply = m_proxy.Delete(m_fileNames);
        break;

    case FileEngine::MoveMode:
        reply = m_proxy.Move(m_fileNames, m_destDirectory);
        break;

    case FileEngine::CopyMode:
        reply = m_proxy.Copy(m_fileNames, m_destDirectory);
        break;

    case FileEngine::IdleMode:
        qmlInfo(this) << "FileWorker run in IdleMode, should not happen";
        return;
    }
    
    if (reply.isError()) {
        qCWarning(diskUsage) << "Uanble to invoke remote file operation:" << reply.error().message();
    } else {
        if (reply.isFinished()) {
            m_operation = reply.value();
        } else {
            QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<uint> reply = *watcher;
                m_operation = reply.value();
                watcher->deleteLater();
            });
        }
    }
}

void FileWorker::fileOperationFailed(unsigned id, const QStringList &paths, unsigned errorCode)
{
    if (id == m_operation) {
        // Report this as finished now, as we don't want to emit the done signal later
        handleFinished();

        FileEngine::Error result = static_cast<FileEngine::Error>(errorCode);
        if (result == FileEngine::ErrorOperationCancelled) {
            emit cancelled();
        } else {
            QString fileName;
            if (!paths.isEmpty()) {
                fileName = paths.first();
            }
            emit error(result, fileName);
        }
    } else if (m_operation != 0) {
        qCWarning(diskUsage) << Q_FUNC_INFO << "Unknown operation:" << id << "!=" << m_operation;
    }
}

void FileWorker::fileOperationSucceeded(unsigned id, const QStringList &paths)
{
    if (id == m_operation) {
        if (m_mode == FileEngine::DeleteMode) {
            foreach (const QString &path, paths)
                emit fileDeleted(path);
        }
    } else if (m_operation != 0) {
        qCWarning(diskUsage) << Q_FUNC_INFO << "Unknown operation:" << id << "!=" << m_operation;
    }
}

void FileWorker::operationFinished(unsigned id)
{
    if (id == m_operation) {
        handleFinished();

        emit done();
    } else if (m_operation != 0) {
        qCWarning(diskUsage) << Q_FUNC_INFO << "Unknown operation:" << id << "!=" << m_operation;
    }
}

void FileWorker::run()
{
    FileOperations::ContinueFunc continueFn = [this]() { return m_cancelled.loadAcquire() == KeepRunning; };

    QStringList succeededPaths, failedPaths;
    FileOperations::PathResultFunc pathResultFn = [&succeededPaths,&failedPaths](const QString &path, bool success) {
        if (success) {
            succeededPaths.append(path);
        } else {
            failedPaths.append(path);
        }
    };

    FileEngine::Error result = FileEngine::NoError;

    switch (m_mode) {
    case FileEngine::DeleteMode:
        result = FileOperations::deleteFiles(m_fileNames, pathResultFn, continueFn);
        break;

    case FileEngine::MoveMode:
        result = FileOperations::moveFiles(m_fileNames, m_destDirectory, pathResultFn, continueFn);
        break;

    case FileEngine::CopyMode:
        result = FileOperations::copyFiles(m_fileNames, m_destDirectory, pathResultFn, continueFn);
        break;

    case FileEngine::IdleMode:
        qmlInfo(this) << "FileWorker run in IdleMode, should not happen";
        return;
    }

    if (m_mode == FileEngine::DeleteMode) {
        // Report any files that have been deleted
        foreach (const QString &path, succeededPaths)
            emit fileDeleted(path);
    }

    if (m_cancelled.loadAcquire() == Cancelled) {
        emit cancelled();
        m_cancelled.storeRelease(KeepRunning);
    } else {
        if (result == FileEngine::NoError) {
            emit done();
        } else {
            QString fileName;
            if (!failedPaths.isEmpty()) {
                fileName = failedPaths.first();
            }
            emit error(result, fileName);
        }
    }
}

bool FileWorker::waitForOperation(unsigned id)
{
    m_operation = id;

    // Wait for the operation to be completed (perpetually?)
    while (m_operation == id) {
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents | QEventLoop::ExcludeUserInputEvents);
    }
    return true;
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

