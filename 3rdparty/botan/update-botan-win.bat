REM Download and setup botan to compile.
REM I had to also install 7zip
set PATH=%PATH%;C:\Program Files\7-Zip;
set PATH=%PATH%;C:\Qt\6.3.0\mingw_64\bin;
if exist %CD%\libBotan cd %CD%\libBotan
if exist %CD%\botan RMDIR /S /Q %CD%\botan
curl -L -o 3.0.0-alpha1.zip https://github.com/randombit/botan/archive/3.0.0-alpha1.zip
7z.exe x 3.0.0-alpha1.zip
del /Q 3.0.0-alpha1.zip
rename botan-3.0.0-alpha1 botan
