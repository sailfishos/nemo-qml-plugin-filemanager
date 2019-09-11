include (../common.pri)

TEMPLATE = aux

QML_FILES =  tst_*.qml
OTHER_FILES += $${QML_FILES}

target.files = $${QML_FILES} folder hiddenfolder protectedfolder
target.path = /usr/lib/$${PACKAGENAME}-tests/auto

INSTALLS += target
