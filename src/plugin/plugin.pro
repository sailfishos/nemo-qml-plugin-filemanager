TARGET = nemofilemanager
PLUGIN_IMPORT_PATH = Nemo/FileManager

TEMPLATE = lib
CONFIG += qt plugin hide_symbols
QT = \
    core \
    concurrent \
    dbus \
    qml

CONFIG += link_pkgconfig

contains(CONFIG, desktop) {
    DEFINES *= DESKTOP
}

QMAKE_CXXFLAGS += -Wparentheses -Werror -Wfatal-errors

PKGCONFIG += KF5Archive

# Drop any library linkage we dont actually need
QMAKE_LFLAGS *= -Wl,--as-needed

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += qmldir plugins.qmltypes
qmldir.path +=  $$target.path
INSTALLS += qmldir

qmltypes.commands = qmlplugindump -nonrelocatable Nemo.FileManager 1.0 > $$PWD/plugins.qmltypes
QMAKE_EXTRA_TARGETS += qmltypes

system(qdbusxml2cpp -c FileOperationsProxy -p fileoperationsproxy.h:fileoperationsproxy.cpp ../../dbus/org.nemomobile.FileOperations.xml)

SOURCES += archiveinfo.cpp \
    archivemodel.cpp \
    fileengine.cpp \
    filemodel.cpp \
    fileoperations.cpp \
    fileoperationsproxy.cpp \
    filewatcher.cpp \
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
    filewatcher.h \
    fileworker.h \
    statfileinfo.h \
    filemanagerglobal.h

INCLUDEPATH += $$PWD ../shared
VPATH += ../shared

OTHER_FILES += ../../example/*/*.qml

LIBS += -L.. -lfilemanager
