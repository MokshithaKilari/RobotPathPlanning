@echo off
setlocal
echo ============================================================
echo  Building Intelligent Robot Path Planning (Release)
echo ============================================================
if not exist build mkdir build
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed. Make sure MinGW-w64 and CMake are in PATH.
    pause
    exit /b 1
)
cmake --build build --config Release -j
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Compilation failed.
    pause
    exit /b 1
)
echo.
echo [SUCCESS] Built build\RobotPathPlanning.exe
echo Running core unit tests...
ctest --test-dir build --output-on-failure
pause
