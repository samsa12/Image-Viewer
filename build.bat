@echo off
REM Pure C Image Viewer - Build Script
REM Requires: Visual Studio Build Tools or MinGW

echo ========================================
echo  Pure C Image Viewer - Build Script
echo ========================================
echo.

REM Check for compiler
where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using MSVC compiler...
    goto :msvc_build
)

where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Using GCC compiler...
    goto :gcc_build
)

echo ERROR: No C compiler found!
echo Please install one of the following:
echo   - Visual Studio Build Tools (cl.exe)
echo   - MinGW-w64 (gcc.exe)
echo.
pause
exit /b 1

:msvc_build
echo Compiling with MSVC...
cl /nologo /O2 /W3 ^
    /Fe:ImageViewer.exe ^
    src\main.c src\image_loader.c src\renderer.c src\file_browser.c ^
    /I lib ^
    user32.lib gdi32.lib shell32.lib comdlg32.lib ^
    /link /SUBSYSTEM:WINDOWS
if %ERRORLEVEL% NEQ 0 goto :error
goto :success

:gcc_build
echo Compiling with GCC...
gcc -O2 -Wall -mwindows ^
    -o ImageViewer.exe ^
    src/main.c src/image_loader.c src/renderer.c src/file_browser.c ^
    resource.o ^
    -I lib ^
    -lgdi32 -lshell32 -lcomdlg32
if %ERRORLEVEL% NEQ 0 goto :error
goto :success

:error
echo.
echo ========================================
echo  BUILD FAILED!
echo ========================================
pause
exit /b 1

:success
echo.
echo ========================================
echo  BUILD SUCCESSFUL!
echo  Output: ImageViewer.exe
echo ========================================
echo.

REM Cleanup MSVC temporary files
del *.obj 2>nul

echo Run ImageViewer.exe to start the application.
pause
