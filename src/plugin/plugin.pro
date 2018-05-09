TARGET = nemofilemanager
PLUGIN_IMPORT_PATH = Nemo/FileManager

TEMPLATE = lib
CONFIG += qt plugin hide_symbols c++11
QT += qml dbus concurrent

CONFIG += link_pkgconfig

contains(CONFIG, desktop) {
    DEFINES *= DESKTOP
} else {
    PKGCONFIG += contactcache-qt5
}

QMAKE_CXXFLAGS += -Wparentheses -Werror -Wfatal-errors

PKGCONFIG += KF5Archive

# Drop any library linkage we dont actually need (such as contactcache-qt5)
QMAKE_LFLAGS *= -Wl,--as-needed

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

system(qdbusxml2cpp -c FileOperationsProxy -p fileoperationsproxy.h:fileoperationsproxy.cpp ../../dbus/org.nemomobile.FileOperations.xml)

SOURCES += archiveinfo.cpp \
    archivemodel.cpp \
    fileengine.cpp \
    filemodel.cpp \
    fileoperations.cpp \
    fileoperationsproxy.cpp \
    fileworker.cpp \
    plugin.cpp \
    statfileinfo.cpp

HEADERS += archiveinfo.h \
    archivemodel_p.h \
    archivemodel.h \
    fileengine.h \
    filemodel.h \
    fileoperations.h \
    fileoperationsproxy.h \
    fileworker.h \
    statfileinfo.h

INCLUDEPATH += $$PWD ../shared
VPATH += ../shared

OTHER_FILES += ../../example/*/*.qml
