QT       -= core gui

TARGET = arsenic_core
TEMPLATE = lib
# Use this for static botan rather than the default dynamic
CONFIG += c++14 warn_off #static

unix {

        equals(QMAKE_CXX, clang++) {
             message(Botan clang)
             SOURCES += linux_clang64/botan_all.cpp
             
             HEADERS += linux_clang64/botan_all.h
    
                    }
     
        equals(QMAKE_CXX, g++) {
            message(Botan gcc)
            SOURCES += linux_gcc64/botan_all.cpp
             
            HEADERS += linux_gcc64/botan_all.h
            }

}#end linux

win32-g++ {
    QMAKE_CXXFLAGS += -Wa,-mbig-obj

             SOURCES += win_mingw64/botan_all.cpp

             HEADERS += win_mingw64/botan_all.h
               

}

#INSTALL Linux
    target.path = /usr/lib/
    INSTALLS += target
