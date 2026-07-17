@echo off
setlocal EnableExtensions
pushd "%~dp0"

if not defined BUILD_DIR (
    if exist "build-vs17\CMakeCache.txt" (
        set "BUILD_DIR=build-vs17"
    ) else (
        set "BUILD_DIR=build"
    )
)
if not defined INSTALL_DIR set "INSTALL_DIR=dist"
set "CONFIG=Release"
set "ACTION=%~1"
set "INTERACTIVE=0"

if not defined ACTION (
    set "INTERACTIVE=1"
    goto menu
)

if /I "%ACTION%"=="help" goto help
if /I "%ACTION%"=="--help" goto help
if /I "%ACTION%"=="-h" goto help

if /I "%ACTION%"=="debug" (
    set "ACTION=build"
    set "CONFIG=Debug"
    goto dispatch
)
if /I "%ACTION%"=="release" (
    set "ACTION=build"
    set "CONFIG=Release"
    goto dispatch
)

if not "%~2"=="" set "CONFIG=%~2"
goto dispatch

:menu
cls
echo ========================================
echo  Plasmoid Visualizer - Build Options
echo ========================================
echo  Build directory: %BUILD_DIR%
echo.
echo  [1] Build Release
echo  [2] Build Debug
echo  [3] Rebuild Release
echo  [4] Build Release and run tests
echo  [5] Package Release to dist\
echo  [6] Build Windows installer
echo  [7] Configure CMake
echo  [8] Clean Release build
echo  [0] Exit
echo.
set /P "CHOICE=Select an option: "

if "%CHOICE%"=="1" (
    set "ACTION=build"
    set "CONFIG=Release"
    goto dispatch
)
if "%CHOICE%"=="2" (
    set "ACTION=build"
    set "CONFIG=Debug"
    goto dispatch
)
if "%CHOICE%"=="3" (
    set "ACTION=rebuild"
    set "CONFIG=Release"
    goto dispatch
)
if "%CHOICE%"=="4" (
    set "ACTION=test"
    set "CONFIG=Release"
    goto dispatch
)
if "%CHOICE%"=="5" (
    set "ACTION=package"
    set "CONFIG=Release"
    goto dispatch
)
if "%CHOICE%"=="6" (
    set "ACTION=installer"
    set "CONFIG=Release"
    goto dispatch
)
if "%CHOICE%"=="7" (
    set "ACTION=configure"
    goto dispatch
)
if "%CHOICE%"=="8" (
    set "ACTION=clean"
    set "CONFIG=Release"
    goto dispatch
)
if "%CHOICE%"=="0" goto success

echo.
echo Invalid selection.
pause
goto menu

:dispatch
if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo ERROR: Configuration must be Debug or Release, not "%CONFIG%".
    goto failure
)

if /I "%ACTION%"=="configure" call :configure& goto finish
if /I "%ACTION%"=="build" call :build& goto finish
if /I "%ACTION%"=="rebuild" call :rebuild& goto finish
if /I "%ACTION%"=="test" call :test& goto finish
if /I "%ACTION%"=="package" call :package& goto finish
if /I "%ACTION%"=="installer" call :installer& goto finish
if /I "%ACTION%"=="clean" call :clean& goto finish

echo ERROR: Unknown action "%ACTION%".
echo.
goto help_error

:configure
echo.
echo [Configure] Generating build files in %BUILD_DIR%\...
cmake -S . -B "%BUILD_DIR%"
exit /b %ERRORLEVEL%

:ensure_configured
if exist "%BUILD_DIR%\CMakeCache.txt" exit /b 0
echo Build directory is not configured yet.
call :configure
exit /b %ERRORLEVEL%

:build
call :ensure_configured
if errorlevel 1 exit /b 1
echo.
echo [Build] Building %CONFIG%...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --parallel
exit /b %ERRORLEVEL%

:rebuild
call :ensure_configured
if errorlevel 1 exit /b 1
echo.
echo [Rebuild] Cleaning and building %CONFIG%...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --clean-first --parallel
exit /b %ERRORLEVEL%

:test
call :build
if errorlevel 1 exit /b 1
echo.
echo [Test] Running %CONFIG% tests...
ctest --test-dir "%BUILD_DIR%" -C "%CONFIG%" --output-on-failure
exit /b %ERRORLEVEL%

:package
call :build
if errorlevel 1 exit /b 1
echo.
echo [Package] Installing %CONFIG% files into %INSTALL_DIR%\...
cmake --install "%BUILD_DIR%" --config "%CONFIG%" --prefix "%INSTALL_DIR%"
exit /b %ERRORLEVEL%

:installer
set "CONFIG=Release"
call :package
if errorlevel 1 exit /b 1

call :find_iscc
if not defined ISCC (
    echo.
    echo ERROR: Inno Setup 6 was not found.
    echo Install it or add ISCC.exe to PATH.
    exit /b 1
)

echo.
echo [Installer] Compiling installer.iss...
"%ISCC%" installer.iss
if errorlevel 1 exit /b 1

echo.
echo Installer output:
dir /B installer_output\*.exe 2>nul
exit /b 0

:find_iscc
set "ISCC="
for %%I in (ISCC.exe) do set "ISCC=%%~$PATH:I"
if defined ISCC exit /b 0
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if defined ISCC exit /b 0
if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
exit /b 0

:clean
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo Build directory is not configured; there is nothing to clean.
    exit /b 0
)
echo.
echo [Clean] Cleaning %CONFIG%...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target clean
exit /b %ERRORLEVEL%

:finish
if errorlevel 1 goto failure

:success
echo.
echo ========================================
echo  SUCCESS
echo ========================================
set "EXIT_CODE=0"
goto end

:failure
echo.
echo ========================================
echo  BUILD FAILED
echo ========================================
set "EXIT_CODE=1"
goto end

:help_error
call :print_help
set "EXIT_CODE=1"
goto end

:help
call :print_help
set "EXIT_CODE=0"
goto end

:print_help
echo Plasmoid Visualizer build script
echo.
echo Usage:
echo   build_installer.bat                       Interactive menu
echo   build_installer.bat configure             Configure build\
echo   build_installer.bat build [Debug^|Release]
echo   build_installer.bat debug                 Build Debug alias
echo   build_installer.bat release               Build Release alias
echo   build_installer.bat rebuild [Debug^|Release]
echo   build_installer.bat test [Debug^|Release]
echo   build_installer.bat package [Debug^|Release]
echo   build_installer.bat installer             Release package and installer
echo   build_installer.bat clean [Debug^|Release]
echo.
echo Optional environment overrides:
echo   set BUILD_DIR=build-custom
echo   set INSTALL_DIR=dist-custom
exit /b 0

:end
if "%INTERACTIVE%"=="1" pause
popd
exit /b %EXIT_CODE%
