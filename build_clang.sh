mkdir build_clang
cd build_clang
qmake -o Makefile ../arsenic.pro -spec linux-clang CONFIG+=qtquickcompiler
make
