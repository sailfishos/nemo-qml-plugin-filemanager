/*
 * Copyright (C) 2018 Jolla Ltd.
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

#include "filewatcher.h"
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlInfo>

FileWatcher::FileWatcher(QObject *parent)
    : QObject(parent)
    , m_exists(false)
    , m_file(new QFile(this))
    , m_watcher(new QFileSystemWatcher(this))
{
    connect(m_watcher, SIGNAL(directoryChanged(QString)), this, SLOT(testFileExists()));
}

FileWatcher::~FileWatcher()
{
}

void FileWatcher::testFileExists()
{
    bool exists = m_file->exists();
    if (m_exists != exists) {
        m_exists = exists;
        emit existsChanged();
    }
}

bool FileWatcher::exists() const
{
    return m_exists;
}

QString FileWatcher::fileName() const
{
    return m_file->fileName();
}

void FileWatcher::setFileName(const QString &fileName)
{
    if (m_file->fileName() != fileName) {
        if (!m_file->fileName().isEmpty()) {
            m_watcher->removePath(m_file->fileName());
        }
        m_file->setFileName(fileName);
        if (!fileName.isEmpty()) {
            QFileInfo fileInfo(fileName);
            QString absolutePath = fileInfo.absolutePath();
            if (!m_watcher->addPath(absolutePath)) {
                qmlInfo(this) << "FileWatcher: watching folder " << absolutePath << " failed";
            }
        }
        testFileExists();
        emit fileNameChanged();
    }
}

bool FileWatcher::testFileExists(const QString &fileName) const
{
    return QFile::exists(fileName);
}
