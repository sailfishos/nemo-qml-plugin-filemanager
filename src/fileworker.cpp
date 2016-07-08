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
#include <QCoreApplication>

#include <sys/types.h>
#include <sys/wait.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

namespace {

bool switchUser(QString username)
{
    const QByteArray utf8UserName(username.toUtf8());
    const char *user = utf8UserName.constData();

    struct passwd pwd;
    char buf[1024];
    struct passwd *result = 0;

    if (::getpwnam_r(user, &pwd, buf, sizeof(buf), &result) != 0) {
        qWarning() << "Unable to find user:" << username << ::strerror(errno);
        return false;
    }
    if (::initgroups(user, pwd.pw_gid) != 0) {
        qWarning() << "Unable to initgroups for user:" << username << ::strerror(errno);
        return false;
    }
    if (::setgid(pwd.pw_gid) != 0) {
        qWarning() << "Unable to setgid for user:" << username << "group:" << pwd.pw_gid <<::strerror(errno);
        return false;
    }
    if (::setuid(pwd.pw_uid) != 0) {
        qWarning() << "Unable to setuid for user:" << username << ::strerror(errno);
        return false;
    }

    qWarning() << "Executing as:" << username;
    return true;
}

bool writeData(int fd, const QByteArray &data)
{
    if (data.isEmpty())
        return true;

    int rv = 0;
    do {
        rv = ::write(fd, data.constData(), data.size());
        if (rv > 0)
            return true;
    } while (rv == -1 && errno == EINTR);

    return false;
}

template<typename ChildFunc, typename ParentFunc>
bool runForkTask(QString asUser, ChildFunc child, ParentFunc parent)
{
    // Fork a child process, switch to the indicated user, then perform the operation
    int pipefd[2] = { -1, -1 };
    if (::pipe(pipefd) != 0) {
        qWarning() << "pipe failed:" << ::strerror(errno);
        return false;
    }

    int pid = ::fork();
    if (pid == -1) {
        qWarning() << "fork failed:" << ::strerror(errno);
        return false;
    }

    const int readHandle = pipefd[0];
    const int writeHandle = pipefd[1];

    if (pid == 0) {
        // Child process - close the read handle
        ::close(readHandle);

        if (switchUser(asUser)) {
            child(writeHandle);
        } else {
            const QString s(QString("error:%1:\n").arg(static_cast<int>(FileEngine::ErrorUserChangeFailed)));
            writeData(writeHandle, s.toUtf8());
        }

        ::close(writeHandle);
        ::_exit(EXIT_SUCCESS);
    } else {
        // Parent process - close the write handle
        ::close(writeHandle);

        // Read any responses from the child
        QString buffered;

        int rv = 0;
        do {
            char buf[1024];
            buf[1023] = '\0';
            rv = ::read(readHandle, buf, sizeof(buf) - 1);
            if (rv > 0) {
                buf[rv] = '\0';
                buffered.append(QString::fromUtf8(buf));

                int index = buffered.indexOf('\n');
                while (index >= 0) {
                    const QString line(buffered.left(index));
                    buffered = buffered.mid(index + 1);
                    index = buffered.indexOf('\n');

                    parent(line);
                }
            }
        } while (rv > 0);

        if (!buffered.isEmpty()) {
            qWarning() << "Incomplete child response:" << buffered;
        }
        ::close(readHandle);

        // Wait for the child to finish
        while (true) {
            int childStatus = 0;
            int rv = ::waitpid(pid, &childStatus, 0);
            if (rv == pid) {
                if (WIFEXITED(childStatus)) {
                    if (WEXITSTATUS(childStatus) != 0) {
                        qWarning() << "abnormal child return code:" << WEXITSTATUS(childStatus);
                    }
                } else {
                    qWarning() << "abnormal child status:" << childStatus;
                }
                break;
            } else if (errno != EINTR) {
                qWarning() << "waitpid failed:" << ::strerror(errno);
                break;
            }
        }
    }
    return true;
}

template<typename Task>
bool runForkTask(QString asUser, Task task, bool &rv)
{
    rv = false;

    auto runTask = [&task](int writeHandle) {
        if (task()) {
            writeData(writeHandle, QByteArray("true\n"));
        }
    };
    auto reportResults = [&rv](QString line) {
        if (line.startsWith("true")) {
            rv = true;
        }
    };

    return runForkTask(asUser, runTask, reportResults);
}

}

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

void FileWorker::startDeleteFiles(QStringList fileNames, QString asUser)
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
    startOperation(asUser);
}

void FileWorker::startCopyFiles(QStringList fileNames, QString destDirectory, QString asUser)
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
    startOperation(asUser);
}

void FileWorker::startMoveFiles(QStringList fileNames, QString destDirectory, QString asUser)
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
    startOperation(asUser);
}

void FileWorker::cancel()
{
    m_cancelled.storeRelease(Cancelled);
}

bool FileWorker::mkdir(QString path, QString name, QString asUser)
{
    bool rv = false;

    QDir dir(path);
    auto task = [&dir, &name]() { return dir.mkdir(name); };

    if (asUser.isEmpty()) {
        rv = task();
    } else {
        // Ensure that we're not running a thread, as that would confuse the fork/wait
        wait();
        if (!runForkTask(asUser, task, rv)) {
            emit error(FileEngine::ErrorUserChangeFailed, "");
        }
    }
    return rv;
}

bool FileWorker::rename(QString oldPath, QString newPath, QString asUser)
{
    bool rv = false;

    QFile file(oldPath);
    auto task = [&file, &newPath]() { return file.rename(newPath); };

    if (asUser.isEmpty()) {
        rv = task();
    } else {
        // Ensure that we're not running a thread, as that would confuse the fork/wait
        wait();
        if (!runForkTask(asUser, task, rv)) {
            emit error(FileEngine::ErrorUserChangeFailed, "");
        }
    }
    return rv;
}

bool FileWorker::setPermissions(QString path, QFileDevice::Permissions p, QString asUser)
{
    bool rv = false;

    QFile file(path);
    auto task = [&file, p]() { return file.setPermissions(p); };

    if (asUser.isEmpty()) {
        rv = task();
    } else {
        // Ensure that we're not running a thread, as that would confuse the fork/wait
        wait();
        if (!runForkTask(asUser, task, rv)) {
            emit error(FileEngine::ErrorUserChangeFailed, "");
        }
    }
    return rv;
}

void FileWorker::startOperation(QString asUser)
{
    if (asUser.isEmpty()) {
        // Run threaded
        start();
        return;
    }

    auto runTask = [this](int writeHandle) {
        // Report signals by sending back to parent process
        connect(this, &FileWorker::fileDeleted, [writeHandle](QString fileName) {
            const QString s(QString("deleted:%1\n").arg(fileName));
            writeData(writeHandle, s.toUtf8());
        });
        connect(this, &FileWorker::error, [writeHandle](FileEngine::Error error, QString fileName) {
            const QString s(QString("error:%1:%2\n").arg(static_cast<int>(error)).arg(fileName));
            writeData(writeHandle, s.toUtf8());
        });
        connect(this, &FileWorker::done, [writeHandle]() {
            const QByteArray ba("done\n");
            writeData(writeHandle, ba);
        });

        run();
    };

    auto reportResults = [this](QString line) {
        if (line.startsWith("deleted:")) {
            const QString fileName(line.mid(8));
            emit fileDeleted(fileName);
        } else if (line.startsWith("error:")) {
            reportError(line);
        } else if (line.startsWith("done")) {
            emit done();
        }
    };

    if (!runForkTask(asUser, runTask, reportResults)) {
        emit error(FileEngine::ErrorUserChangeFailed, "");
    }
}

void FileWorker::reportError(QString line)
{
    const QString remainder(line.mid(6));
    const int index = remainder.indexOf(':');
    const QString errorCode(remainder.left(index));
    const QString fileName(remainder.mid(index + 1));
    emit error(static_cast<FileEngine::Error>(errorCode.toInt()), fileName);
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
    }

    emit done();
}

void FileWorker::copyOrMoveFiles()
{
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

