@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
set PATH=C:\Program Files\CMake\bin;C:\Users\Nate\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%
cd /d C:\GrooveEngineRnB
cmake --build build --config Release -- -j1 2>&1
