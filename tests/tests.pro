TEMPLATE = aux

QML_FILES =  auto/tst_*.qml
OTHER_FILES += $${QML_FILES}

target.files = $${QML_FILES} auto/folder auto/hiddenfolder auto/protectedfolder
target.path = /opt/tests/nemo-qml-plugins/filemanager/auto

definition.files = tests.xml
definition.path = /opt/tests/nemo-qml-plugins/filemanager/test-definition

INSTALLS += target definition
