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

#include "fileoperationsservice.h"
#include "fileoperationsadaptor.h"

#include "fileengine.h"

namespace {

const QString serviceName(QStringLiteral("org.nemomobile.FileOperations"));

const int LingerTimeMs = 2 * 60 * 1000;

}

struct OperationRequest
{
    enum Type { Copy, Move, Delete, Mkdir, Rename, SetPermissions };

    unsigned id;
    Type type;
    QStringList paths;
    QString destination;
    unsigned mask;
};

struct OperationResponse
{
    FileEngine::Error error;
    QStringList successPaths;
    QStringList failedPaths;

    bool continueOperation()
    {
        if (cancelled.loadAcquire() == 0)
            return true;

        // This request has been cancelled
        error = FileEngine::ErrorOperationCancelled;
        return false;
    }

    void cancel()
    {
        cancelled.storeRelease(1);
    }

private:
    QAtomicInt cancelled;
};

class WorkerThread : public QThread
{
    Q_OBJECT

    FileOperationsService *service;

public:
    WorkerThread(FileOperationsService *service) : QThread(service) , service(service) {}

    void run() { service->processRequests(); }
};

FileOperationsService::FileOperationsService()
    : QObject(0)
    , adaptor(new FileOperationsAdaptor(this))
    , requestId(0)
    , serviceFinished(false)
{
    if (!QDBusConnection::sessionBus().registerService(serviceName)) {
        qFatal("Unable to register service: %s", qPrintable(serviceName));
    }

    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/"), this)) {
        qFatal("Unable to register server object");
    }

    connect(this, &FileOperationsService::requestCompleted, this, &FileOperationsService::requestFinished, Qt::QueuedConnection);

    // Create worker threads to perform the file opertaions
    for (int i = 0, n = QThread::idealThreadCount() * 2; i < n; ++i) {
        WorkerThread *thread = new WorkerThread(this);
        thread->start();
    }
}

unsigned FileOperationsService::Copy(const QStringList &paths, const QString &destination)
{
    OperationRequest request;

    request.type = OperationRequest::Copy;
    request.paths = paths;
    request.destination = destination;

    return enqueueRequest(request);
}

unsigned FileOperationsService::Move(const QStringList &paths, const QString &destination)
{
    OperationRequest request;

    request.type = OperationRequest::Move;
    request.paths = paths;
    request.destination = destination;

    return enqueueRequest(request);
}

unsigned FileOperationsService::Delete(const QStringList &paths)
{
    OperationRequest request;

    request.type = OperationRequest::Delete;
    request.paths = paths;

    return enqueueRequest(request);
}

unsigned FileOperationsService::Mkdir(const QString &path, const QString &destination)
{
    OperationRequest request;

    request.type = OperationRequest::Mkdir;
    request.paths = QStringList() << path;
    request.destination = destination;

    return enqueueRequest(request);
}

unsigned FileOperationsService::Rename(const QString &oldPath, const QString &newPath)
{
    OperationRequest request;

    request.type = OperationRequest::Rename;
    request.paths = QStringList() << oldPath;
    request.destination = newPath;

    return enqueueRequest(request);
}

unsigned FileOperationsService::SetPermissions(const QString &path, unsigned mask)
{
    OperationRequest request;

    request.type = OperationRequest::SetPermissions;
    request.paths = QStringList() << path;
    request.mask = mask;

    return enqueueRequest(request);
}

void FileOperationsService::Cancel(unsigned handle)
{
    QMutexLocker lock(&requestMutex);

    // Is there a response for this request?
    QMap<unsigned, OperationResponse *>::iterator it = responses.find(handle);
    if (it == responses.end()) {
        // No response - the request has been handled already
        qDebug() << "Cannot cancel completed operation" << handle;
    } else {
        OperationResponse *response = *it;

        // See if this request is still pending
        QList<OperationRequest *>::iterator it = std::find_if(requests.begin(), requests.end(), [handle](OperationRequest *request) { return request->id == handle; });
        if (it != requests.end()) {
            // Just remove the request and report it as cancelled
            OperationRequest *request = *it;
            requests.erase(it);

            response->error = FileEngine::ErrorOperationCancelled;
            response->failedPaths = request->paths;
            emit requestCompleted(request->id);
            qDebug() << "Cancelled pending operation" << handle;

            delete request;
        } else {
            qDebug() << "Cancelling active operation" << handle;

            // The request is in progress - flag the reponse as cancelled
            response->cancel();
        }
    }
}

unsigned FileOperationsService::enqueueRequest(const OperationRequest &operation, bool validRequest)
{
    OperationResponse *response = new OperationResponse;
    OperationRequest *request = 0;

    if (validRequest) {
        request = new OperationRequest(operation);
    } else {
        response->failedPaths = operation.paths;
        response->error = FileEngine::ErrorInvalidRequest;
    }

    unsigned id = 0;

    {
        QMutexLocker lock(&requestMutex);

        id = ++requestId;
        responses.insert(id, response);

        if (validRequest) {
            // Enqueue the request for processing
            request->id = id;
            requests.prepend(request);

            // Wake a worker to process the request
            requestAvailable.wakeOne();
        }
    }

    if (!validRequest) {
        // Report failure asynchronously
        emit requestCompleted(id);
    }

    timer.stop();

    return id;
}

void FileOperationsService::requestFinished(unsigned id)
{
    OperationResponse *response = 0;
    bool pendingResponses = true;

    {
        QMutexLocker lock(&requestMutex);

        QMap<unsigned, OperationResponse *>::iterator it = responses.find(id);
        if (it != responses.end()) {
            response = *it;
            responses.erase(it);
            pendingResponses = !responses.isEmpty();
        }
    }

    if (response) {
        if (!response->successPaths.isEmpty()) {
            emit adaptor->Succeeded(id, response->successPaths);
        }
        if (!response->failedPaths.isEmpty()) {
            emit adaptor->Failed(id, response->failedPaths, static_cast<unsigned>(response->error));
        }
        emit adaptor->Finished(id);

        delete response;
    } else {
        qWarning() << Q_FUNC_INFO << "Invalid id:" << id;
    }

    if (!pendingResponses) {
        // Schedule termination if no new requests are received
        timer.start(LingerTimeMs, this);
    }
}

void FileOperationsService::processRequests()
{
    while (true) {
        requestMutex.lock();

        // Take the next request
        while (!serviceFinished && requests.isEmpty()) {
            requestAvailable.wait(&requestMutex);
        }
        if (serviceFinished) {
            requestMutex.unlock();
            return;
        }

        // Process the next requested URI
        OperationRequest *request = requests.takeLast();
        OperationResponse *response = 0;

        QMap<unsigned, OperationResponse *>::iterator it = responses.find(request->id);
        if (it == responses.end()) {
            qWarning() << Q_FUNC_INFO << "Invalid id:" << request->id << "for request";
        } else {
            response = *it;
        }

        // Start another handler if there are waiting requests
        if (!requests.isEmpty()) {
            requestAvailable.wakeOne();
        }
        requestMutex.unlock();

        processOperation(request, response);
        emit requestCompleted(request->id);

        delete request;
    }
}

void FileOperationsService::processOperation(OperationRequest *request, OperationResponse *response)
{
    Q_UNUSED(request)
    Q_UNUSED(response)
}

void FileOperationsService::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timer.timerId()) {
        timer.stop();

        qWarning() << "FileOperationsService expired after linger period";

        {
            QMutexLocker lock(&requestMutex);
            serviceFinished = true;
            requestAvailable.wakeAll();
        }

        QThread::msleep(1000);
        emit serviceExpired();
    }
}

#include "fileoperationsservice.moc"
