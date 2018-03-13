TEMPLATE = app
TARGET = fileoperationsd

QT += dbus
CONFIG += c++11

CONFIG += link_pkgconfig link_prl
QMAKE_CXXFLAGS += -Wparentheses -Werror -Wfatal-errors

packagesExist(qt5-boostable) {
    DEFINES += HAS_BOOSTER
    PKGCONFIG += qt5-boostable
} else {
    warning("qt5-boostable not available; startup times will be slower")
}

system(qdbusxml2cpp -c FileOperationsAdaptor -a fileoperationsadaptor.h:fileoperationsadaptor.cpp ../../dbus/org.nemomobile.FileOperations.xml)

HEADERS =\
    fileoperationsadaptor.h \
    fileoperationsservice.h \
    fileoperations.h

SOURCES =\
    fileoperationsadaptor.cpp \
    fileoperationsservice.cpp \
    fileoperations.cpp \
    main.cpp

INCLUDEPATH += ../plugin ../shared
VPATH += ../shared

SERVICE_FILE = ../../dbus/org.nemomobile.FileOperations.service
INTERFACE_FILE = ../../dbus/org.nemomobile.FileOperations.xml
OTHER_FILES += $$SERVICE_FILE $$INTERFACE_FILE

service.files = $$SERVICE_FILE
service.path  = /usr/share/dbus-1/services/

interface.files = $$INTERFACE_FILE
interface.path  = /usr/share/dbus-1/interfaces/

target.path = /usr/bin

INSTALLS += service interface target
