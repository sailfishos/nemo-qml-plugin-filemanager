include (../common.pri)

QT += testlib qml dbus
QT -= gui

TEMPLATE = app
TARGET = ut_diskusage

target.path = /opt/tests/$${PACKAGENAME}

contains(cov, true) {
    message("Coverage options enabled")
    QMAKE_CXXFLAGS += --coverage
    QMAKE_LFLAGS += --coverage
}

CONFIG += link_prl
DEFINES += UNIT_TEST
QMAKE_EXTRA_TARGETS = check

check.depends = $$TARGET
check.commands = LD_LIBRARY_PATH=../../../$$[QT_INSTALL_LIBS] ./$$TARGET

INCLUDEPATH += ../../src/plugin/

SOURCES += ut_diskusage.cpp
HEADERS += ut_diskusage.h

SOURCES += ../../src/plugin/diskusage.cpp
HEADERS += ../../src/plugin/diskusage.h
HEADERS += ../../src/plugin/diskusage_p.h

INSTALLS += target
