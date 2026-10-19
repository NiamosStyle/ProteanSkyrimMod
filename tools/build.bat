@echo off
rem Configure + build the Protean SKSE plugin (MSVC x64 Release, Ninja). Optional arg: MO2 mod folder to deploy into.
rem   tools\build.bat "C:\Users\user\AppData\Local\ModOrganizer\Skyrim Special Edition\mods\Protean"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "PATH=C:\Program Files\CMake\bin;%PATH%"
if "%VCPKG_ROOT%"=="" set "VCPKG_ROOT=C:\vcpkg"
cd /d "%~dp0.."
set "DEPLOY=-DPROTEAN_DEPLOY_DIR="
if not "%~1"=="" set "DEPLOY=-DPROTEAN_DEPLOY_DIR=%~1"
cmake --preset release "%DEPLOY%"
if errorlevel 1 exit /b 1
cmake --build --preset release
if errorlevel 1 exit /b 1
