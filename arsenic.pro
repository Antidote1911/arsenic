OTHER_FILES += \
    defaults.pri
    
TEMPLATE = subdirs

SUBDIRS += 3rdparty \
           arscore \
           arsenic_gui

arscore.depends = 3rdparty
arsenic.depends = arscore
arsenic_gui.depends = arscore

