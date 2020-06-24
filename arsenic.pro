OTHER_FILES += \
    defaults.pri

TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS += 3rdparty \
           arsenic \
           tests

arsenic.depends = 3rdparty
tests.depends = 3rdparty

DISTFILES += .appveyor.yml \
             .travis.yml \
             .clang-format \
             LICENSE \
             README.md \
             botan_license.txt \
             screenshots/main_dark.png \
             screenshots/hash.png \
             screenshots/cryptopad_light.png \
             screenshots/pass_gen.png
