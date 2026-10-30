@echo off
rem Requires g++ in PATH (e.g. w64devkit: https://github.com/skeeto/w64devkit/releases)
cd /d "%~dp0"
g++ -std=c++17 -O1 main.cpp -o rest_server.exe -lws2_32 -pthread
g++ -std=c++17 tests\variant_tests.cpp -o variant_tests.exe
echo Done.
pause
