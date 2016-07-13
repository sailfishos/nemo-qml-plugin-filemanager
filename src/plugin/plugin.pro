TARGET = nemofilemanager
PLUGIN_IMPORT_PATH = Nemo/FileManager

TEMPLATE = lib
CONFIG += qt plugin hide_symbols c++11
QT += qml

CONFIG += link_pkgconfig
PKGCONFIG += contactcache-qt5

# Drop any library linkage we dont actually need (such as contactcache-qt5)
QMAKE_LFLAGS *= -Wl,--as-needed

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$target.path
INSTALLS += qmldir

SOURCES += plugin.cpp filemodel.cpp fileengine.cpp \
           fileworker.cpp statfileinfo.cpp

HEADERS += filemodel.h fileengine.h fileworker.h \
           statfileinfo.h

INCLUDEPATH += $$PWD

OTHER_FILES += ../../example/*/*.qml
