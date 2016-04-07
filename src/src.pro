TARGET = nemofilemanager
PLUGIN_IMPORT_PATH = Nemo/FileManager

TEMPLATE = lib
CONFIG += qt plugin hide_symbols c++11
QT += qml

CONFIG += link_pkgconfig

# We just need the include path from contactcache, not the library itself
QMAKE_CXXFLAGS *= $$system(pkg-config --cflags contactcache-qt5)

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
