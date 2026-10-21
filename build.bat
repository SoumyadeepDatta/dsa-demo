@echo off
rem Builds the demo with MinGW g++ on Windows.
rem ws2_32 is the Windows socket library used by web_server.cpp.

g++ -std=c++11 -O2 -Wall -o sharding_demo.exe src\*.cpp -lws2_32
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)
echo Built sharding_demo.exe - run it and open http://localhost:8080
