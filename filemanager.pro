TEMPLATE = subdirs

src_plugins.subdir = src/plugin
src_plugins.target = sub-plugins
src_plugins.depends += src

src_daemon.subdir = src/daemon
src_daemon.target = sub-daemon
src_plugins.depends += src

tests.depends = src src_plugins

SUBDIRS = src src_plugins src_daemon tests

OTHER_FILES = rpm/*.spec
