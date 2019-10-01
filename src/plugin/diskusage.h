/*
 * Copyright (C) 2015 - 2019 Jolla Ltd.
 * Copyright (c) 2019 Open Mobile Platform LLC.
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
 *   * Neither the name of Nemo Mobile nor the names of its contributors
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

#ifndef DISKUSAGE_H
#define DISKUSAGE_H

#include <QObject>
#include <QVariant>
#include <QJSValue>
#include <QScopedPointer>

#include <filemanagerglobal.h>

class DiskUsagePrivate;

class FILEMANAGER_EXPORT DiskUsage: public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(DiskUsage)

    // True while calculation takes place
    Q_PROPERTY(bool working READ working NOTIFY workingChanged)

    Q_PROPERTY(QVariantMap result READ result NOTIFY resultChanged)
    Q_PROPERTY(Status status READ status WRITE setStatus NOTIFY statusChanged)


public:
    explicit DiskUsage(QObject *parent = nullptr);
    virtual ~DiskUsage();

    enum Status{
        Idle,
        Calculating,
        Counting
    };
    Q_ENUM(Status)

    // Calculate the disk usage of the given paths, then call
    // callback with a QVariantMap (mapping paths to usages in bytes)
    Q_INVOKABLE void calculate(const QStringList &paths, QJSValue callback);
    Q_INVOKABLE void fileCount(const QString &path, QJSValue callback, bool recursive = false);
    QVariantMap result() const;

signals:
    void workingChanged();
    void resultChanged();
    void statusChanged(Status status);

signals:
    void submit(QStringList paths, QJSValue *callback, QPrivateSignal);
    void startCounting(const QString &path, QJSValue *callback, bool recursive, QPrivateSignal);

private slots:
    void finished(QVariantMap usage, QJSValue *callback);
    void countingFinished(const int &counter, QJSValue *callback);

private:
    bool working() const;
    Status status() const;

    void setStatus(Status status);
    void setWorking(bool working);

private:
    QScopedPointer<DiskUsagePrivate> const d_ptr;   
};

#endif /* DISKUSAGE_H */
