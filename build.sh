mkdir build
cd build
qmake -o Makefile ../arsenic.pro -spec linux-g++ CONFIG+=qtquickcompiler
make
