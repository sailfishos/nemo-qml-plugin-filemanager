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

import QtTest 1.0
import QtQuick 2.0
import Nemo.FileManager 1.0

Item {
    property int lastError

    FileModel {
        id: fileModel
        path: "folder"
        active: true
    }

    Connections {
        target: FileEngine
        onError: lastError = error
    }

    Repeater {
        id: repeater
        model: fileModel
        Item {
            property string fileName: model.fileName
            property string mimeType: model.mimeType
            property int size: model.size
            property bool isDir: model.isDir
        }
    }

    SignalSpy {
        id: workerDoneSpy
        target: FileEngine
        signalName: "workerDone"
    }

    SignalSpy {
        id: errorSpy
        target: FileEngine
        signalName: "error"
    }

    SignalSpy {
        id: fileCountSpy
        target: repeater
        signalName: "countChanged"
    }

    resources: TestCase {
        name: "FileEngine"

        function init() {
        }
        function cleanup() {
            fileModel.path = "folder"
        }

        function test_copying() {
            // initial state
            verify(!FileEngine.clipboardContainsCopy)
            compare(FileEngine.clipboardCount, 0)

            var originalPath = fileModel.path

            for (var i = 0; i < 2; ++i) {
                var nonprivileged = (i == 1)

                // copy second file
                fileModel.path = originalPath
                tryCompare(fileModel, "populated", true)

                var fileName = repeater.itemAt(1).fileName
                FileEngine.copyFiles([ fileModel.fileNameAt(1) ])

                verify(FileEngine.clipboardContainsCopy)
                compare(FileEngine.clipboardCount, 1)

                // go to sub-folder, paste the file
                fileModel.path = fileModel.appendPath("subfolder")
                wait(0)
                compare(fileModel.count, 1)
                FileEngine.pasteFiles(fileModel.path, nonprivileged)

                // wait for the file operation to finish
                workerDoneSpy.wait()
                workerDoneSpy.clear()

                // check that new file has appeared
                var newFileName = fileModel.path + "/" + fileName
                wait(0)
                compare(fileModel.count, 2)
                compare(fileModel.fileNameAt(0), newFileName)

                // delete newly copied file
                FileEngine.deleteFiles([ newFileName ], nonprivileged)

                // wait for the deletion to finish
                workerDoneSpy.wait()
                wait(0)
                compare(fileModel.count, 1)
            }
        }

        function test_mkdir() {

            var originalPath = fileModel.path

            for (var i = 0; i < 2; ++i) {
                var nonprivileged = (i == 1)

                fileModel.path = originalPath
                fileModel.path = fileModel.appendPath("subfolder")
                wait(0)
                compare(fileModel.count, 1)

                fileCountSpy.clear()

                // create folder
                var name = "yetanotherdirectory"
                var success = FileEngine.mkdir(fileModel.path, name, nonprivileged)
                verify(success)

                // check that folder got created
                fileCountSpy.wait()
                wait(0)
                compare(fileModel.count, 2)
                compare(repeater.itemAt(1).fileName, name)

                fileCountSpy.clear()

                // delete the folder
                FileEngine.deleteFiles([ fileModel.fileNameAt(1) ], nonprivileged)

                // wait for the deletion to finish
                fileCountSpy.wait()
                wait(0)
                compare(fileModel.count, 1)
            }
        }

        function test_errors() {

            var originalPath = fileModel.path

            for (var i = 0; i < 2; ++i) {
                var nonprivileged = (i == 1)

                lastError = FileEngine.NoError

                // Copying folder inside itself
                FileEngine.copyFiles([ fileModel.fileNameAt(3) ])
                FileEngine.pasteFiles([ fileModel.fileNameAt(3) ])
                compare(lastError, FileEngine.ErrorCannotCopyIntoItself)

                // Copying a file to protected path
                var protectedFolder =  fileModel.parentPath() + "/protectedfolder"
                FileEngine.copyFiles([ fileModel.fileNameAt(0) ])
                FileEngine.pasteFiles([ protectedFolder ])
                errorSpy.clear()
                errorSpy.wait()
                compare(lastError, FileEngine.ErrorCopyFailed)

                // Copying a folder to protected path
                FileEngine.copyFiles([ fileModel.fileNameAt(3) ])
                FileEngine.pasteFiles([ protectedFolder ])
                errorSpy.clear()
                errorSpy.wait()
                compare(lastError, FileEngine.ErrorFolderCopyFailed)

                // Creating a folder inside protected folder
                FileEngine.mkdir(protectedFolder, "test")
                compare(lastError, FileEngine.ErrorFolderCreationFailed)

                // Deleting a protected file
                FileEngine.deleteFiles( [ protectedFolder ])
                errorSpy.clear()
                errorSpy.wait()
                compare(lastError, FileEngine.ErrorDeleteFailed)
            }
        }
    }
}
