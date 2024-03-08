TEMPLATE = lib
TARGET = filemanager

CONFIG += qt create_pc create_prl no_install_prl
QT = qml core dbus

SOURCES += \
    plugin/diskusage.cpp \
    plugin/diskusage_impl.cpp

PUBLIC_HEADERS = \
    plugin/diskusage.h \
    plugin/filemanagerglobal.h

HEADERS += \
    $$PUBLIC_HEADERS \
    plugin/diskusage_p.h \

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
QMAKE_PKGCONFIG_REQUIRES = Qt5Core Qt5Qml

INSTALLS += target develheaders pkgconfig

INCLUDEPATH += $$PWD/plugin
