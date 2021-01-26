include(common.pri)

TEMPLATE = subdirs
SUBDIRS = auto \
    ut_diskusage

OTHER_FILES += tests.xml.template

LIBDIR = $$[QT_INSTALL_LIBS]
system(sed -e s/@PACKAGENAME@/$${PACKAGENAME}/g -e s/@LIBDIR@/$$re_escape($$replace(LIBDIR, /, \/))/g tests.xml.template > tests.xml)

xml.path = /usr/share/$${PACKAGENAME}-tests
xml.files = tests.xml

INSTALLS += xml
