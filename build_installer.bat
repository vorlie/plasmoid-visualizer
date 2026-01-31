@echo off
REM Build installer script for Plasmoid Visualizer

echo ========================================
echo Plasmoid Visualizer - Installer Builder
echo ========================================
echo.

REM Step 1: Build the application
echo [1/3] Building Release...
cmake --build build --config Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    exit /b 1
)

REM Step 2: Package distribution
echo.
echo [2/3] Creating distribution package...
cmake --install build --config Release --prefix dist
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Packaging failed!
    exit /b 1
)

REM Step 3: Compile installer
echo.
echo [3/3] Compiling installer...
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Installer compilation failed!
    echo Make sure Inno Setup is installed at the default location.
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS! Installer created.
echo ========================================
echo Location: installer_output\
dir /B installer_output\*.exe
echo.
