include(common.pri)

TEMPLATE = subdirs
SUBDIRS = auto \
    ut_diskusage

OTHER_FILES += tests.xml.template

system(sed -e s/@PACKAGENAME@/$${PACKAGENAME}/g tests.xml.template > tests.xml)

xml.path = /usr/share/$${PACKAGENAME}-tests
xml.files = tests.xml

INSTALLS += xml
