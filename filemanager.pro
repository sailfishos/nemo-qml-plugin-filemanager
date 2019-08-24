TEMPLATE = subdirs

src_plugins.subdir = src/plugin
src_plugins.target = sub-plugins
src_plugins.depends += src tests

src_daemon.subdir = src/daemon
src_daemon.target = sub-daemon

unit_tests.subdir = tests/unit
unit_tests.target = sub-unit

SUBDIRS = src src_plugins src_daemon unit_tests tests

OTHER_FILES = rpm/*.spec
