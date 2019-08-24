TEMPLATE = lib
TARGET = filemanager

CONFIG += qt create_pc create_prl no_install_prl c++11
QT += qml dbus systeminfo
QT -= gui

CONFIG += link_pkgconfig
PKGCONFIG += mlite5

SOURCES += \
    plugin/diskusage.cpp \
    plugin/diskusage_impl.cpp

PUBLIC_HEADERS = \
    plugin/diskusage.h

HEADERS += \
    $$PUBLIC_HEADERS \
    plugin/diskusage_p.h \
    plugin/filemanagerglobal.h

DEFINES += \
    FILEMANAGER_BUILD_LIBRARY

target.path = $$[QT_INSTALL_LIBS]

develheaders.path = /usr/include/filemanager
develheaders.files = $$PUBLIC_HEADERS

pkgconfig.files = $$PWD/pkgconfig/nemofilemanager.pc
pkgconfig.path = $$[QT_INSTALL_LIBS]/pkgconfig

QMAKE_PKGCONFIG_NAME = lib$$TARGET
QMAKE_PKGCONFIG_VERSION = $$VERSION
QMAKE_PKGCONFIG_DESCRIPTION = Filemanager application development files
QMAKE_PKGCONFIG_LIBDIR = $$target.path
QMAKE_PKGCONFIG_INCDIR = $$develheaders.path
QMAKE_PKGCONFIG_DESTDIR = pkgconfig
QMAKE_PKGCONFIG_REQUIRES = Qt5Core Qt5DBus profile nemomodels-qt5 libsailfishkeyprovider connman-qt5

INSTALLS += target develheaders pkgconfig

INCLUDEPATH += $$PWD/plugin
