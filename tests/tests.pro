include(common.pri)

TEMPLATE = subdirs
SUBDIRS = auto \
    ut_diskusage

OTHER_FILES += tests.xml.template

system(sed -e s/@PACKAGENAME@/$${PACKAGENAME}/g tests.xml.template > tests.xml)

xml.path = /opt/tests/$${PACKAGENAME}
xml.files = tests.xml

INSTALLS += xml
