TARGET = nemofilemanager
PLUGIN_IMPORT_PATH = Nemo/FileManager

TEMPLATE = lib
CONFIG += qt plugin hide_symbols c++11
QT += qml dbus

CONFIG += link_pkgconfig

contains(CONFIG, desktop) {
    DEFINES *= DESKTOP
} else {
    PKGCONFIG += contactcache-qt5
}

# Drop any library linkage we dont actually need (such as contactcache-qt5)
QMAKE_LFLAGS *= -Wl,--as-needed

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

system(qdbusxml2cpp -c FileOperationsProxy -p fileoperationsproxy.h:fileoperationsproxy.cpp ../../dbus/org.nemomobile.FileOperations.xml)

SOURCES += fileengine.cpp \
    filemodel.cpp \
    fileoperations.cpp \
    fileoperationsproxy.cpp \
    fileworker.cpp \
    plugin.cpp \
    statfileinfo.cpp

HEADERS += fileengine.h \
    filemodel.h \
    fileoperations.h \
    fileoperationsproxy.h \
    fileworker.h \
    statfileinfo.h

INCLUDEPATH += $$PWD ../shared
VPATH += ../shared

OTHER_FILES += ../../example/*/*.qml
