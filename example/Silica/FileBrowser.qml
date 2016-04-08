import QtQuick 2.0
import Nemo.FileManager 1.0
import Sailfish.Silica 1.0

ApplicationWindow {
    id: root

    initialPage: Component {
        Page {
            SilicaListView {
                id: lisView

                anchors.fill: parent
                model: fileModel

                header: PageHeader {
                    title: fileModel.directoryName
                }

                delegate: ListItem {
                    height: Theme.itemSizeMedium

                    Image {
                        id: icon
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        source: "image://theme/icon-m-" + (model.isDir ? "folder" : "file-other") + (highlighted ? '?' + Theme.highlightColor : '')
                    }

                    Item {
                        id: description
                        anchors {
                            verticalCenter: parent.verticalCenter
                            verticalCenterOffset: -Theme.paddingMedium
                            right: parent.right
                            rightMargin: Theme.horizontalPageMargin
                            left: icon.right
                            leftMargin: Theme.paddingMedium
                        }
                        height: fileNameLabel.height
                        baselineOffset: fileNameLabel.baselineOffset
                        Label {
                            id: fileNameLabel
                            text: model.fileName
                            color: highlighted ? Theme.highlightColor : Theme.primaryColor
                            truncationMode: TruncationMode.Fade
                            width: parent.width - sizeLabel.width - Theme.paddingMedium
                        }
                        Label {
                            id: sizeLabel
                            text: Format.formatFileSize(model.size)
                            font.pixelSize: Theme.fontSizeExtraSmall
                            anchors.right: parent.right
                            anchors.baseline: parent.baseline
                            color: highlighted ? Theme.highlightColor : Theme.primaryColor
                        }
                    }

                    Row {
                        spacing: Theme.paddingSmall
                        anchors {
                            top: description.bottom
                            right: description.right
                        }

                        Label {
                            opacity: 0.6
                            color: highlighted ? Theme.highlightColor : Theme.primaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            anchors.verticalCenter: parent.verticalCenter
                            text: model.mimeType
                            visible: !model.isDir
                        }

                        Label {
                            opacity: 0.6
                            color: highlighted ? Theme.highlightColor : Theme.primaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                            anchors.verticalCenter: parent.verticalCenter
                            text: Format.formatDate(model.modified, Format.Timepoint)
                        }
                    }

                    onClicked: {
                        if (model.isDir) {
                            fileModel.path = model.absolutePath
                        }
                    }
                }

                VerticalScrollDecorator {}

                PullDownMenu {
                    id: menu

                    property var action
                    onActiveChanged: {
                        if (!active && action) {
                            action()
                            action = undefined
                        }
                    }

                    MenuItem {
                        visible: fileModel.includeDirectories
                        text: 'Sort Directories: ' + (fileModel.directorySort == FileModel.SortDirectoriesBeforeFiles ? 'first' : 'last')
                        onClicked: menu.action = function() { fileModel.directorySort = (fileModel.directorySort == FileModel.SortDirectoriesBeforeFiles ? FileModel.SortDirectoriesAfterFiles : FileModel.SortDirectoriesBeforeFiles) }
                    }
                    MenuItem {
                        text: 'Show Directories: ' + (fileModel.includeDirectories ? 'on' : 'off')
                        onClicked: menu.action = function() { fileModel.includeDirectories = !fileModel.includeDirectories }
                    }
                    MenuItem {
                        text: 'Sort by: ' + (fileModel.sortBy == FileModel.SortByModified ? 'Time'
                                                               : (fileModel.sortBy == FileModel.SortByName ? 'Name'
                                                                                    : (fileModel.sortBy == FileModel.SortByExtension ? 'Type' : 'Size')))
                        onClicked: menu.action = function() {
                            fileModel.sortBy = (fileModel.sortBy == FileModel.SortByModified ? FileModel.SortByName 
                                                                  : (fileModel.sortBy == FileModel.SortByName ? FileModel.SortByExtension
                                                                                       : (fileModel.sortBy == FileModel.SortByExtension ? FileModel.SortBySize :FileModel.SortByModified)))
                        }
                    }
                    MenuItem {
                        text: 'Order: ' + (fileModel.sortOrder == Qt.AscendingOrder ? 'Ascending' : 'Descending')
                        onClicked: menu.action = function() { fileModel.sortOrder = (fileModel.sortOrder == Qt.AscendingOrder ? Qt.DescendingOrder : Qt.AscendingOrder) }
                    }
                }
            }
        }
    }

    FileModel {
        id: fileModel
        active: true
        includeDirectories: true
        includeParentDirectory: true
        directorySort: FileModel.SortDirectoriesBeforeFiles
        sortBy: FileModel.SortByName
        sortOrder: Qt.AscendingOrder
        path: "."
    }
}
