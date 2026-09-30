@echo off
if not exist .build mkdir .build
cl /nologo /utf-8 /O2 /W4 /WX /EHsc /std:c++17 /Fo.build\ tests\integration.cpp /link /SUBSYSTEM:CONSOLE /WX User32.lib Shell32.lib Ole32.lib Gdi32.lib Advapi32.lib /OUT:.build\integration.exe
if errorlevel 1 exit /b 1
.build\integration.exe

