REM Download and setup botan to compile.
REM I had to also install 7zip
set PATH=%PATH%;C:\Program Files\7-Zip;
set PATH=%PATH%;C:\Qt\Qt5.11\Tools\mingw530_32\bin;
if exist %CD%\libBotan cd %CD%\libBotan
if exist %CD%\botan RMDIR /S /Q %CD%\botan
curl -L -o 2.14.0.zip https://github.com/randombit/botan/archive/2.14.0.zip
7z x 2.14.0.zip
del /Q 2.14.0.zip
rename botan-2.14.0 botan
