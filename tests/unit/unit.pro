# based on tests.pro from libprofile-qt

PACKAGENAME = nemo-qml-plugin-filemanager

QT += testlib qml dbus
QT -= gui

system(sed -e s/@PACKAGENAME@/$${PACKAGENAME}/g tests.xml.template > tests.xml)

TEMPLATE = app
TARGET = ut_diskusage

target.path = /usr/lib/$${PACKAGENAME}-tests

xml.path = /usr/share/$${PACKAGENAME}-tests
xml.files = tests.xml

contains(cov, true) {
    message("Coverage options enabled")
    QMAKE_CXXFLAGS += --coverage
    QMAKE_LFLAGS += --coverage
}

CONFIG += link_prl
DEFINES += UNIT_TEST
QMAKE_EXTRA_TARGETS = check

check.depends = $$TARGET
check.commands = LD_LIBRARY_PATH=../../lib ./$$TARGET

INCLUDEPATH += ../../src/plugin/

SOURCES += ut_diskusage.cpp
HEADERS += ut_diskusage.h

SOURCES += ../../src/plugin/diskusage.cpp
HEADERS += ../../src/plugin/diskusage.h
HEADERS += ../../src/plugin/diskusage_p.h

INSTALLS += target xml
