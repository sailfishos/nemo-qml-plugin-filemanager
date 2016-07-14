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

#ifndef FILEOPERATIONS_H
#define FILEOPERATIONS_H

#include <QFileDevice>
#include <QString>
#include <QStringList>

#include <functional>

#include "fileengine.h"

struct FileOperations
{
    typedef std::function<bool()> ContinueFunc;
    typedef std::function<void(const QString &, bool)> PathResultFunc;

    FileOperations() = delete;
    FileOperations(const FileOperations &) = delete;

    static bool deleteFile(const QString &path);
    static bool copyOverwrite(const QString &src, const QString &dest);
    static bool copyDirRecursively(const QString &srcDirectory, const QString &destDirectory, ContinueFunc continueOperation = ContinueFunc());

    static FileEngine::Error copyFiles(const QStringList &paths, const QString &destination, PathResultFunc pathResult = PathResultFunc(), ContinueFunc continueOperation = ContinueFunc());
    static FileEngine::Error moveFiles(const QStringList &paths, const QString &destination, PathResultFunc pathResult = PathResultFunc(), ContinueFunc continueOperation = ContinueFunc());
    static FileEngine::Error deleteFiles(const QStringList &paths, PathResultFunc pathResult = PathResultFunc(), ContinueFunc continueOperation = ContinueFunc());
    static FileEngine::Error createDirectory(const QString &path, const QString &directory, PathResultFunc pathResult = PathResultFunc());
    static FileEngine::Error renameFile(const QString &path, const QString &newPath, PathResultFunc pathResult = PathResultFunc());
    static FileEngine::Error chmodFile(const QString &path, QFileDevice::Permissions perm, PathResultFunc pathResult = PathResultFunc());
};

#endif
