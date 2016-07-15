/*
 * Copyright (C) 2016 Jolla Ltd.
 * Contact: Matt Vogt <matthew.vogt@jollamobile.com.com>
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


#include "fileoperations.h"

#include <QDir>
#include <QFile>


bool FileOperations::deleteFile(const QString &path)
{
    QFileInfo info(path);
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

bool FileOperations::copyOverwrite(const QString &src, const QString &dest)
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

bool FileOperations::copyDirRecursively(const QString &srcDirectory, const QString &destDirectory, ContinueFunc continueOperation)
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
        // stop if cancelled
        if (continueOperation && !continueOperation()) {
            return false;
        }

        const QString fileName = names.at(i);
        const QString spath = srcDir.absoluteFilePath(fileName);
        const QString dpath = destDir.absoluteFilePath(fileName);
        if (!copyOverwrite(spath, dpath)) {
            return false;
        }
    }

    // copy dirs
    names = srcDir.entryList(QDir::NoDotAndDotDot | QDir::AllDirs);
    for (int i = 0 ; i < names.count() ; ++i) {
        // stop if cancelled
        if (continueOperation && !continueOperation()) {
            return false;
        }

        const QString fileName = names.at(i);
        const QString spath = srcDir.absoluteFilePath(fileName);
        const QString dpath = destDir.absoluteFilePath(fileName);
        if (!copyDirRecursively(spath, dpath, continueOperation)) {
            return false;
        }
    }

    return true;
}

FileEngine::Error FileOperations::copyFiles(const QStringList &paths, const QString &destination, PathResultFunc pathResult, ContinueFunc continueOperation)
{
    FileEngine::Error rv = FileEngine::NoError;

    QDir destDir(destination);

    QStringList::const_iterator it = paths.cbegin(), end = paths.cend();
    for ( ; it != end; ++it) {
        if (!continueOperation || continueOperation()) {
            const QString &path(*it);

            QFileInfo fileInfo(path);
            const QString newName = destDir.absoluteFilePath(fileInfo.fileName());

            // move or copy and stop if errors
            QFile file(path);
            if (fileInfo.isDir()) {
                if (!copyDirRecursively(path, newName, continueOperation)) {
                    rv = FileEngine::ErrorFolderCopyFailed;
                    break;
                }
            } else if (!copyOverwrite(path, newName)) {
                rv = FileEngine::ErrorCopyFailed;
                break;
            }

            if (pathResult)
                pathResult(path, true);
        }
    }

    // Report any unprocessed paths as failed
    if (pathResult)
        std::for_each(it, end, [&pathResult](const QString &path) { pathResult(path, false); });

    return rv;
}

FileEngine::Error FileOperations::moveFiles(const QStringList &paths, const QString &destination, PathResultFunc pathResult, ContinueFunc continueOperation)
{
    FileEngine::Error rv = FileEngine::NoError;

    QDir destDir(destination);

    QStringList::const_iterator it = paths.cbegin(), end = paths.cend();
    for ( ; it != end; ++it) {
        if (!continueOperation || continueOperation()) {
            const QString &path(*it);

            QFileInfo fileInfo(path);
            const QString newName = destDir.absoluteFilePath(fileInfo.fileName());

            // move or copy and stop if errors
            QFile file(path);
            if (fileInfo.isSymLink()) {
                // move symlink by creating a new link and deleting the old one
                QFile targetFile(fileInfo.symLinkTarget());
                if (!targetFile.link(newName)) {
                    rv = FileEngine::ErrorMoveFailed;
                    break;
                }
                if (!file.remove()) {
                    rv = FileEngine::ErrorMoveFailed;
                    break;
                }
            } else if (!file.rename(newName)) {
                rv = FileEngine::ErrorMoveFailed;
                break;
            }

            if (pathResult)
                pathResult(path, true);
        }
    }

    // Report any unprocessed paths as failed
    if (pathResult)
        std::for_each(it, end, [&pathResult](const QString &path) { pathResult(path, false); });

    return rv;
}

FileEngine::Error FileOperations::deleteFiles(const QStringList &paths, PathResultFunc pathResult, ContinueFunc continueOperation)
{
    FileEngine::Error rv = FileEngine::NoError;

    QStringList::const_iterator it = paths.cbegin(), end = paths.cend();
    for ( ; it != end; ++it) {
        if (!continueOperation || continueOperation()) {
            // delete file and stop if errors
            const QString &path(*it);
            if (deleteFile(path)) {
                if (pathResult)
                    pathResult(path, true);
            } else {
                rv = FileEngine::ErrorDeleteFailed;
                break;
            }
        }
    }

    // Report any unprocessed paths as failed
    if (pathResult)
        std::for_each(it, end, [&pathResult](const QString &path) { pathResult(path, false); });

    return rv;
}

FileEngine::Error FileOperations::createDirectory(const QString &path, const QString &directory, PathResultFunc pathResult)
{
    FileEngine::Error rv = FileEngine::NoError;

    QDir dir(directory);
    if (!dir.mkdir(path)) {
        if (pathResult)
            pathResult(path, false);
        rv = FileEngine::ErrorFolderCreationFailed;
    } else {
        if (pathResult)
            pathResult(path, true);
    }

    return rv;
}

FileEngine::Error FileOperations::renameFile(const QString &path, const QString &newPath, PathResultFunc pathResult)
{
    FileEngine::Error rv = FileEngine::NoError;

    QFile file(path);
    if (!file.rename(newPath)) {
        if (pathResult)
            pathResult(path, false);
        rv = FileEngine::ErrorRenameFailed;
    } else {
        if (pathResult)
            pathResult(path, true);
    }

    return rv;
}

FileEngine::Error FileOperations::chmodFile(const QString &path, QFileDevice::Permissions perm, PathResultFunc pathResult)
{
    FileEngine::Error rv = FileEngine::NoError;

    QFile file(path);
    if (!file.setPermissions(perm)) {
        if (pathResult)
            pathResult(path, false);
        rv = FileEngine::ErrorChmodFailed;
    } else {
        if (pathResult)
            pathResult(path, true);
    }

    return rv;
}

