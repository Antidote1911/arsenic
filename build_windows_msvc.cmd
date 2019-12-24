echo on

SET project_dir="%cd%"

echo "Setting up environment..."
set PATH=%QTPATH%\bin\;C:\Qt\QtIFW-3.0.1\bin\;%PATH%
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

echo "Building Arsenic..."
qmake arsenic.pro QMAKE_CXXFLAGS+=/MP
nmake
