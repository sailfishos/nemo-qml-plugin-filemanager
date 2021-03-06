/*
 * Copyright (C) 2015 Jolla Ltd.
 * Contact: Thomas Perl <thomas.perl@jolla.com>
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

#ifndef DISKUSAGE_P_H
#define DISKUSAGE_P_H

#include <QObject>
#include <QVariant>
#include <QJSValue>
#include <diskusage.h>

class DiskUsageWorker : public QObject
{
    Q_OBJECT

public:
    explicit DiskUsageWorker(QObject *parent=0);
    virtual ~DiskUsageWorker();

    void scheduleQuit() { m_quit = true; }
    void sheduleStopCounting() { m_stopCounting = true; }

public slots:
    void submit(QStringList paths, QJSValue *callback);
    void startCounting(const QString &path, QJSValue *callback, DiskUsage::Filter filter, bool recursive);

signals:
    void finished(QVariantMap usage, QJSValue *callback);
    void countingFinished(const int &counter, QJSValue *callback);

private:
    QVariantMap calculate(QStringList paths);
    quint64 calculateSize(QString directory, QString *expandedPath = nullptr);
    quint64 calculateRpmSize(const QString &glob);
    quint64 calculateApkdSize(const QString &rest);
    int counting(const QString &path, DiskUsage::Filter filter, bool recursive);

    bool m_quit;
    bool m_stopCounting;
    bool m_apkd_data_queried = false;
    qulonglong m_apkd_app = 0;
    qulonglong m_apkd_data = 0;
    qulonglong m_apkd_run = 0;

    friend class Ut_DiskUsage;
};

#endif /* DISKUSAGE_P_H */
