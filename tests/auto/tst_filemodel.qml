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

    resources: TestCase {
        name: "FileModel"

        function init() {
        }
        function cleanup() {
            fileModel.path = "folder"
        }

        property var results: [
            {fileName: "a", mimeType: "application/x-zerosize", size: 0, isDir: false},
            {fileName: "b", mimeType: "text/plain", size: 2, isDir: false},
            {fileName: "c", mimeType: "text/plain", size: 4, isDir: false},
            {fileName: "subfolder", mimeType: "inode/directory", size: 4096, isDir: true}
        ]

        function test_listing() {
            compare(fileModel.count, 4)

            for (var i = 0; i < fileModel.count; i++) {
                var actual = repeater.itemAt(i)
                var expected = results[i]
                compare(actual.fileName, expected.fileName)
                compare(actual.mimeType, expected.mimeType)
                compare(actual.size, expected.size)
                compare(actual.isDir, expected.isDir)
            }
        }

        function test_navigation() {
            // go to sub-folder
            fileModel.path = fileModel.appendPath("subfolder")
            compare(fileModel.parentPath()  + "/" + "subfolder", fileModel.path)
            compare(fileModel.count, 1)
            compare(repeater.itemAt(0).fileName, "d")

            // return
            fileModel.path = fileModel.parentPath()
            compare(fileModel.count, 4)
            compare(repeater.itemAt(0).fileName, "a")
        }

        function test_errors() {
            compare(lastError, FileModel.NoError)

            fileModel.path = "hiddenfolder"
            compare(lastError, FileModel.ErrorReadNoPermissions)
        }
    }
}

