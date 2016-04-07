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

#ifndef ENGINE_H
#define ENGINE_H

#include <QDir>
#include <QVariant>

class FileWorker;

/**
 * @brief FileEngine to handle file operations, settings and other generic functionality.
 */
class FileEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int clipboardCount READ clipboardCount NOTIFY clipboardCountChanged)
    Q_PROPERTY(bool clipboardContainsCopy READ clipboardContainsCopy NOTIFY clipboardContainsCopyChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(Mode mode READ mode NOTIFY modeChanged)

    Q_ENUMS(Error)
    Q_ENUMS(Mode)
public:
    explicit FileEngine(QObject *parent = 0);
    ~FileEngine();

    enum Error {
        NoError,
        ErrorOperationInProgress,
        ErrorCopyFailed,
        ErrorDeleteFailed,
        ErrorMoveFailed,
        ErrorRenameFailed,
        ErrorCannotCopyIntoItself,
        ErrorFolderCopyFailed,
        ErrorFolderCreationFailed,
        ErrorChmodFailed
    };

    enum Mode {
        IdleMode,
        DeleteMode,
        CopyMode,
        MoveMode
    };

    // properties
    int clipboardCount() const { return m_clipboardFiles.count(); }
    bool clipboardContainsCopy() const { return m_clipboardContainsCopy; }
    bool busy() const;
    Mode mode() const;

    // methods accessible from QML

    // asynch methods send signals when done or error occurs
    Q_INVOKABLE void deleteFiles(QStringList fileNames);
    Q_INVOKABLE void cutFiles(QStringList fileNames);
    Q_INVOKABLE void copyFiles(QStringList fileNames);
    Q_INVOKABLE void pasteFiles(QString destDirectory);

    // cancel asynch methods
    Q_INVOKABLE void cancel();

    // synchronous methods
    Q_INVOKABLE bool exists(QString fileName);
    Q_INVOKABLE bool mkdir(QString path, QString name);
    Q_INVOKABLE bool rename(QString fullOldFileName, QString newName);
    Q_INVOKABLE bool chmod(QString path,
                              bool ownerRead, bool ownerWrite, bool ownerExecute,
                              bool groupRead, bool groupWrite, bool groupExecute,
                              bool othersRead, bool othersWrite, bool othersExecute);


signals:
    void clipboardCountChanged();
    void clipboardContainsCopyChanged();
    void workerDone();
    void error(Error error, QString fileName);
    void fileDeleted(QString fullname);
    void cancelled();
    void busyChanged();
    void modeChanged();

private:
    QStringList m_clipboardFiles;
    bool m_clipboardContainsCopy;
    FileWorker *m_fileWorker;
};

#endif // ENGINE_H
