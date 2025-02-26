include (../common.pri)

TEMPLATE = aux

QML_FILES =  tst_*.qml
OTHER_FILES += $${QML_FILES} folder/* folder/subfolder/*

target.files = $${QML_FILES} folder hiddenfolder protectedfolder
target.path = /opt/tests/$${PACKAGENAME}/auto

INSTALLS += target
