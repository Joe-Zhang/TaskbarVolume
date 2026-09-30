@echo off
if not exist .build mkdir .build
cl /nologo /utf-8 /O2 /W4 /WX /EHsc /std:c++17 /Fo.build\ tests\smoke.cpp /link /SUBSYSTEM:CONSOLE /WX User32.lib /OUT:.build\smoke.exe
if errorlevel 1 exit /b 1
.build\smoke.exe
