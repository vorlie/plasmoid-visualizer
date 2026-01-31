@echo off
REM Build and package script for Plasmoid Visualizer

echo Building Release configuration...
cmake --build build --config Release

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

echo.
echo Creating distribution package...
cmake --install build --config Release --prefix dist

if %ERRORLEVEL% NEQ 0 (
    echo Install failed!
    exit /b 1
)

echo.
echo ===================================
echo Package created successfully!
echo Location: dist\
echo ===================================
echo.
echo Contents:
dir /B dist
echo.
