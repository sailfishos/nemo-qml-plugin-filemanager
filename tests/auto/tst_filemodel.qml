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
    FileModel {
        id: fileModel
        path: "folder"
        active: true
    }

    Repeater {
        id: repeater
        model: fileModel
        Item {
            property string fileName: model.fileName
            property string baseName: model.baseName
            property string extension: model.extension
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
            {fileName: "a", baseName: "a", extension: "", mimeType: "application/x-zerosize", size: 0, isDir: false},
            {fileName: "b", baseName: "b", extension: "", mimeType: "text/plain", size: 2, isDir: false},
            {fileName: "c", baseName: "c", extension: "", mimeType: "text/plain", size: 4, isDir: false},
            {fileName: "subfolder", baseName: "subfolder", extension: "", mimeType: "inode/directory", size: 4096, isDir: true},
            {fileName: ".hidden.xml", baseName: ".hidden", extension: "xml", mimeType: "application/xml", size: 52, isDir: false},
            {fileName: ".tarball.tar.bz2", baseName: ".tarball", extension: "tar.bz2", mimeType: "application/x-bzip2-compressed-tar", size: 109, isDir: false},
            {fileName: ".hidden", baseName: ".hidden", extension: "", mimeType: "text/plain", size: 6, isDir: false}
        ]

        function test_includeFiles() {
            fileModel.includeFiles = false
            wait(0)
            compare(fileModel.count, 1)

            fileModel.includeFiles = true
            wait(0)
            compare(fileModel.count, 4)
        }

        function test_listing() {
            var check = function(indices, name) {
                wait(0)
                compare(fileModel.count, indices.length, name)

                for (var i = 0; i < indices.length; i++) {
                    var message = name + ': failed at index:' + i + ' (result[' + indices[i] + '])'
                    var actual = repeater.itemAt(i)
                    var expected = results[indices[i]]
                    compare(actual.fileName, expected.fileName, message)
                    compare(actual.baseName, expected.baseName, message)
                    compare(actual.extension, expected.extension, message)
                    compare(actual.mimeType, expected.mimeType, message)
                    compare(actual.size, expected.size, message)
                    compare(actual.isDir, expected.isDir, message)
                }
            }

            fileModel.sortBy = FileModel.SortBySize
            fileModel.includeDirectories = false
            check([2, 1, 0], 'Sort by size')

            fileModel.sortOrder = Qt.DescendingOrder
            check([0, 1, 2], 'Reverse by size')

            fileModel.includeHiddenFiles = true
            fileModel.sortBy = FileModel.SortByName
            check([2, 1, 0, 5, 4, 6], 'Reverse by name with hidden')

            fileModel.sortBy = FileModel.SortBySize
            check([0, 1, 2, 6, 4, 5], 'Reverse by size with hidden')

            fileModel.includeHiddenFiles = false
            fileModel.directorySort = FileModel.SortDirectoriesBeforeFiles
            fileModel.includeDirectories = true
            check([3, 0, 1, 2], 'Reverse by size after directories')

            fileModel.directorySort = FileModel.SortDirectoriesAfterFiles
            check([0, 1, 2, 3], 'Reverse by size before directories')

            fileModel.sortOrder = Qt.AscendingOrder
            check([2, 1, 0, 3], 'By size before directories')

            fileModel.sortBy = FileModel.SortByName
            check([0, 1, 2, 3], 'By name before directories')

            fileModel.nameFilters = [ 'b' ]
            check([1, 3], 'Filtered by name')

            fileModel.nameFilters = [ 'a', 'b' ]
            check([0, 1, 3], 'Filtered by names')

            fileModel.nameFilters = [ '[bc]*' ]
            check([1, 2, 3], 'Filtered by glob')

            // An inactive model should still update on configuration change
            fileModel.active = false
            check([1, 2, 3], 'Deactivated')

            fileModel.nameFilters = [ 'c' ]
            check([2, 3], 'Filtered by name while inactive')

            fileModel.active = true
            check([2, 3], 'Filtered by name after reactivation')
        }

        function test_navigation() {
            fileModel.sortBy = FileModel.SortByName
            fileModel.sortOrder = Qt.AscendingOrder
            fileModel.includeDirectories = true
            fileModel.directorySort = FileModel.SortDirectoriesWithFiles
            fileModel.nameFilters = []
            fileModel.active = true

            // go to sub-folder
            fileModel.path = fileModel.appendPath("subfolder")
            wait(0)
            compare(fileModel.parentPath()  + "/" + "subfolder", fileModel.path)
            compare(fileModel.count, 1)
            compare(repeater.itemAt(0).fileName, "d")

            // return
            fileModel.path = fileModel.parentPath()
            wait(0)
            compare(fileModel.count, 4)
            compare(repeater.itemAt(0).fileName, "a")
        }

        function test_errors() {
            compare(fileModel.errorType, FileModel.NoError)

            fileModel.path = "hiddenfolder"
            wait(0)
            compare(fileModel.errorType, FileModel.ErrorReadNoPermissions)
        }
    }
}

