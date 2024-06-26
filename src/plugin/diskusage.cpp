/*
 * Copyright (c) 2015 - 2019 Jolla Ltd.
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

#include "diskusage.h"
#include "diskusage_p.h"

#include <QThread>
#include <QDebug>
#include <QJSEngine>
#include <QDir>
#include <QDirIterator>

Q_LOGGING_CATEGORY(diskUsage, "org.sailfishos.diskusage", QtWarningMsg)

DiskUsageWorker::DiskUsageWorker(QObject *parent)
    : QObject(parent)
    , m_quit(false)
    , m_stopCounting{false}
{
}

DiskUsageWorker::~DiskUsageWorker()
{
}

void DiskUsageWorker::submit(QStringList paths, QJSValue *callback)
{
    emit finished(calculate(paths), callback);
}

void DiskUsageWorker::startCounting(const QString &path, QJSValue *callback, DiskUsage::Filter filter, bool recursive)
{
    emit countingFinished(counting(path, filter, recursive), callback);
}

int DiskUsageWorker::counting(const QString &path, DiskUsage::Filter filter, bool recursive)
{
    m_stopCounting = false;

    QFileInfo fileinfo(path);
    if (!fileinfo.isDir() || !fileinfo.exists())
        return 0;

    QDir::Filters filters = static_cast<QDir::Filter>(filter | QDir::NoDotAndDotDot);
    QDirIterator it(path, filters, recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    int counter = 0;

    while (it.hasNext()) {
        if (m_stopCounting)
            return counter;

        it.next();
        counter++;
    }
    return counter;
}

QVariantMap DiskUsageWorker::calculate(QStringList paths)
{    
    QVariantMap usage;
    // expanded Path places the object in the tree so parents can have it subtracted from its total
    QMap<QString, QString> expandedPaths; // input path -> expanded path
    QMap<QString, QString> originalPaths; // expanded path -> input path

    foreach (const QString &path, paths) {
        QString expandedPath;
        // Pseudo-path for querying RPM database for file sizes
        // ----------------------------------------------------
        // Example path with package name: ":rpm:python3-base"
        // Example path with glob: ":rpm:harbour-*" (will sum up all matching package sizes)
        if (path.startsWith(":rpm:")) {
            QString glob = path.mid(5);
            usage[path] = calculateRpmSize(glob);
            expandedPath = "/usr/" + path;
        } else if (path.startsWith(":apkd:")) {
            // Pseudo-path for querying Android apps' data usage
            QString rest = path.mid(6);
            usage[path] = calculateApkdSize(rest);
            if (rest == "runtime") {
                expandedPath = "/opt/appsupport";
            } else {
                expandedPath = "/home/.appsupport/" + rest;
            }
        } else {
            quint64 size = calculateSize(path, &expandedPath);
            usage[path] = size;
        }

        expandedPaths[path] = expandedPath;
        originalPaths[expandedPath] = path;
        qCDebug(diskUsage) << "Counted" << path << "as" << expandedPath << ":" << usage[path];
        if (m_quit) {
            break;
        }
    }

    // Sort keys in reverse order (so child directories come before their
    // parents, and the calculation is done correctly, no child directory
    // subtracted once too often), for example:
    //  1. a0 = size(/home/<user>/foo/)
    //  2. b0 = size(/home/<user>/)
    //  3. c0 = size(/)
    //
    // This will calculate the following changes in the nested for loop below:
    //  1. b1 = b0 - a0
    //  2. c1 = c0 - a0
    //  3. c2 = c1 - b1
    //
    // Combined and simplified, this will give us the output values:
    //  1. a' = a0
    //  2. b' = b1 = b0 - a0
    //  3. c' = c2 = c1 - b1 = (c0 - a0) - (b0 - a0) = c0 - a0 - b0 + a0 = c0 - b0
    //
    // Or with paths:
    //  1. output(/home/<user>/foo/) = size(/home/<user>/foo/)
    //  2. output(/home/<user>/)     = size(/home/<user>/)     - size(/home/<user>/foo/)
    //  3. output(/)               = size(/)               - size(/home/<user>/)
    QStringList keys;
    foreach (const QString &key, usage.uniqueKeys()) {
        keys << expandedPaths.value(key, key);
    }
    qStableSort(keys.begin(), keys.end(), qGreater<QString>());
    for (int i=0; i<keys.length(); i++) {
        for (int j=i+1; j<keys.length(); j++) {
            QString subpath = keys[i];
            QString path = keys[j];

            if ((subpath.length() > path.length() && subpath.indexOf(path) == 0) || (path == "/")) {
                qlonglong subbytes = usage[originalPaths.value(subpath, subpath)].toLongLong();
                qlonglong bytes = usage[originalPaths.value(path, path)].toLongLong();

                bytes -= subbytes;
                usage[originalPaths.value(path, path)] = bytes;
            }
        }
    }

    return usage;
}

class DiskUsagePrivate
{
    Q_DISABLE_COPY(DiskUsagePrivate)
    Q_DECLARE_PUBLIC(DiskUsage)

    DiskUsage * const q_ptr;

public:
    DiskUsagePrivate(DiskUsage *usage);
    ~DiskUsagePrivate();

private:
    QThread *m_thread;
    DiskUsageWorker *m_worker;
    bool m_working;
    DiskUsage::Status m_status;
    QVariantMap m_result;
};

DiskUsagePrivate::DiskUsagePrivate(DiskUsage *usage)
    : q_ptr(usage)
    , m_thread(new QThread())
    , m_worker(new DiskUsageWorker())
    , m_working(false)
    , m_status(DiskUsage::Idle)
{
    m_worker->moveToThread(m_thread);

    QObject::connect(usage, &DiskUsage::submit,
                     m_worker, &DiskUsageWorker::submit);

    QObject::connect(m_worker, &DiskUsageWorker::finished,
                     usage,  &DiskUsage::finished);

    QObject::connect(m_thread, &QThread::finished,
                     m_worker, &DiskUsageWorker::deleteLater);

    QObject::connect(m_thread, &QThread::finished,
                     m_thread, &QThread::deleteLater);

    QObject::connect(usage, &DiskUsage::startCounting,
                     m_worker, &DiskUsageWorker::startCounting);

    QObject::connect(m_worker, &DiskUsageWorker::countingFinished,
                     usage, &DiskUsage::countingFinished);

    m_thread->start();
}

DiskUsagePrivate::~DiskUsagePrivate()
{
    // Make sure the worker quits as soon as possible
    m_worker->scheduleQuit();
    m_worker->sheduleStopCounting();

    // Tell thread to shut down as early as possible
    m_thread->quit();
}


DiskUsage::DiskUsage(QObject *parent)
    : QObject(parent)
    , d_ptr(new DiskUsagePrivate(this))
{
}

DiskUsage::~DiskUsage()
{
}

void DiskUsage::calculate(const QStringList &paths, QJSValue callback)
{
    QJSValue *cb = nullptr;

    if (!callback.isNull() && !callback.isUndefined() && callback.isCallable()) {
        cb = new QJSValue(callback);
    }

    setStatus(DiskUsage::Calculating);
    setWorking(true);
    emit submit(paths, cb, QPrivateSignal());
}

void DiskUsage::fileCount(const QString &path, QJSValue callback, DiskUsage::Filter filter, bool recursive)
{
    QJSValue *cb = nullptr;

    if (!callback.isNull() && !callback.isUndefined() && callback.isCallable()) {
        cb = new QJSValue(callback);
    }

    setStatus(DiskUsage::Counting);
    emit startCounting(path, cb, filter, recursive, QPrivateSignal());
}

void DiskUsage::finished(QVariantMap usage, QJSValue *callback)
{
    if (callback) {
        QJSValue result = callback->call(QJSValueList() << callback->engine()->toScriptValue(usage));
        if (result.isError()) {
            qCWarning(diskUsage) << "Error on diskusage callback" << result.toString();
        }
        delete callback;
    }

    // the result has been set, so emit resultChanged() even if result was not valid
    Q_D(DiskUsage);
    d->m_result = usage;
    emit resultChanged();

    setStatus(DiskUsage::Idle);
    setWorking(false);
}

void DiskUsage::countingFinished(int counter, QJSValue *callback)
{
    if (callback) {
        QJSValue result = callback->call(QJSValueList() << callback->engine()->toScriptValue(counter));
        if (result.isError()) {
            qCWarning(diskUsage) << "Error on diskusage callback" << result.toString();
        }
        delete callback;
    }

    setStatus(DiskUsage::Idle);
}

bool DiskUsage::working() const
{
    Q_D(const DiskUsage);
    return d->m_working;
}

DiskUsage::Status DiskUsage::status() const
{
    Q_D(const DiskUsage);
    return d->m_status;
}

void DiskUsage::setWorking(bool working)
{
    Q_D(DiskUsage);
    if (d->m_working != working) {
        d->m_working = working;
        emit workingChanged();
    }
}

QVariantMap DiskUsage::result() const
{
    Q_D(const DiskUsage);
    return d->m_result;
}

void DiskUsage::setStatus(DiskUsage::Status status)
{
    Q_D(DiskUsage);
    if (d->m_status == status)
        return;

    d->m_status = status;
    emit statusChanged(d->m_status);
}
