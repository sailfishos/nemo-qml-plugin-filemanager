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

#ifndef FILEWORKER_H
#define FILEWORKER_H

#include <QThread>
#include <QDir>
#include "fileengine.h"

/**
 * @brief FileWorker does delete, copy and move files in the background.
 */
class FileWorker : public QThread
{
    Q_OBJECT

public:
    explicit FileWorker(QObject *parent = 0);
    ~FileWorker();

    // call these to start the thread, returns false if start failed
    void startDeleteFiles(QStringList fileNames, QString asUser);
    void startCopyFiles(QStringList fileNames, QString destDirectory, QString asUser);
    void startMoveFiles(QStringList fileNames, QString destDirectory, QString asUser);

    void cancel();

    // synchronous functions
    bool mkdir(QString path, QString name, QString asUser);
    bool rename(QString oldPath, QString newPath, QString asUser);
    bool setPermissions(QString path, QFileDevice::Permissions p, QString asUser);

    FileEngine::Mode mode() const;

signals:

    // one of these is emitted when thread ends
    void done();
    void cancelled();
    void error(FileEngine::Error error, QString fileName);
    void fileDeleted(QString fullname);
    void modeChanged();

protected slots:
    void handleFinished();

protected:
    void run();

private:
    enum CancelStatus {
        Cancelled = 0,
        KeepRunning = 1
    };

    void startOperation(QString asUser);

    void reportError(QString line);

    bool validateFileNames(const QStringList &fileNames);

    bool deleteFile(QString fileNames);
    void deleteFiles();
    void copyOrMoveFiles();
    bool copyDirRecursively(QString srcDirectory, QString destDirectory);
    bool copyOverwrite(QString src, QString dest);
    void setMode(FileEngine::Mode mode);

    FileEngine::Mode m_mode;
    QStringList m_fileNames;
    QString m_destDirectory;
    QAtomicInt m_cancelled; // atomic so no locks needed
};

#endif // FILEWORKER_H
