TARGET = nemofilemanager
PLUGIN_IMPORT_PATH = Nemo/FileManager

TEMPLATE = lib
CONFIG += qt plugin hide_symbols c++11
QT += qml

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

SOURCES += plugin.cpp filemodel.cpp filedata.cpp engine.cpp fileworker.cpp \
           consolemodel.cpp statfileinfo.cpp globals.cpp

HEADERS += filemodel.h filedata.h engine.h fileworker.h \
           consolemodel.h statfileinfo.cpp globals.h

INCLUDEPATH += $$PWD
