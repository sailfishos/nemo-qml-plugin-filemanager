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

#include "diskusage.h"
#include "diskusage_p.h"

#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QStorageInfo>

quint64 DiskUsageWorker::calculateSize(QString directory, QString *expandedPath)
{
    // In lieu of wordexp(3) support in Qt, fake it
    if (directory.startsWith("~/")) {
        directory = QDir::homePath() + '/' + directory.mid(2);
    }

    if (expandedPath) {
        *expandedPath = directory;
    }

    if (directory == "/") {
        return QStorageInfo::root().bytesTotal() - QStorageInfo::root().bytesAvailable();
    }

    QDir d(directory);
    if (!d.exists() || !d.isReadable()) {
        return 0L;
    }

    QProcess du;
    du.start("du", QStringList() << "-skx" << directory, QIODevice::ReadOnly);
    du.waitForFinished();
    if (du.exitStatus() != QProcess::NormalExit) {
        qCWarning(diskUsage) << "Could not determine size of:" << directory;
        return 0L;
    }
    QStringList size_directory = QString::fromUtf8(du.readAll()).split('\t');

    if (size_directory.size() > 1) {
        return size_directory[0].toULongLong() * 1024;
    }

    return 0L;
}

quint64 DiskUsageWorker::calculateRpmSize(const QString &glob)
{
    QProcess rpm;
    rpm.start("rpm", QStringList() << "-qa" << "--queryformat=%{name}|%{size}\\n" << glob, QIODevice::ReadOnly);
    rpm.waitForFinished();
    if (rpm.exitStatus() != QProcess::NormalExit) {
        qCWarning(diskUsage) << "Could not determine size of RPM packages matching:" << glob;
        return 0L;
    }

    quint64 result = 0L;

    QStringList lines = QString::fromUtf8(rpm.readAll()).split('\n', QString::SkipEmptyParts);
    foreach (const QString &line, lines) {
        int index = line.indexOf('|');
        if (index == -1) {
            qCWarning(diskUsage) << "Could not parse RPM output line:" << line;
            continue;
        }

        QString package = line.left(index);
        result += line.mid(index + 1).toULongLong();
    }

    return result;
}

quint64 DiskUsageWorker::calculateApkdSize(const QString &rest)
{
    if (!m_apkd_data_queried) {
        qCDebug(diskUsage) << "Querying apkd for Android app data usage";
        QDBusInterface apkd("com.jolla.apkd", "/com/jolla/apkd", "com.jolla.apkd", QDBusConnection::sessionBus());
        QDBusReply<QDBusArgument> reply = apkd.call("getAndroidAppDataUsage");
        if (reply.isValid()) {
            int apkd_cache = 0;
            const QDBusArgument output = reply.value();
            output.beginStructure();
            output >> m_apkd_app;
            output >> m_apkd_data;
            output >> apkd_cache;
            // TODO: Add an 'Android app cache' category, and report this separately.
            // But for now add this to the data count
            m_apkd_data += apkd_cache;
            output.endStructure();
            m_apkd_run = calculateSize("/opt/appsupport");
        }
        m_apkd_data_queried = true;
    }

    if (rest == "app") {
        return m_apkd_app;
    } else if (rest == "data") {
        return m_apkd_data;
    } else if (rest == "runtime") {
        return m_apkd_run;
    } else {
        qCWarning(diskUsage) << "Unknown Android usage type: " << rest;
        return 0L;
    }
}
