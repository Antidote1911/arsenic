OTHER_FILES += \
    defaults.pri
    
TEMPLATE = subdirs

SUBDIRS += arscore \
           arsenic \
           arsenic_gui \
           tests

arsenic.depends = arscore
arsenic_gui.depends = arscore
tests.depends = arscore 
