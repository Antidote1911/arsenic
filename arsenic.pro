OTHER_FILES += \
    defaults.pri

TEMPLATE = subdirs
CONFIG  += ordered

SUBDIRS += thirdparty \
           arsenic \
           tests

arsenic.depends = thirdparty
tests.depends = thirdparty

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
