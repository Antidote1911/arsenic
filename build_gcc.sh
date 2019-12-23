mkdir build_gcc
cd build_gcc
qmake -o Makefile ../arsenic.pro -spec linux-g++ CONFIG+=qtquickcompiler
make
