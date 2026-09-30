@echo off
if not exist .build mkdir .build
cl /nologo /utf-8 /O2 /W4 /EHsc /std:c++17 /Fo.build\ ^
    main.cpp audio.cpp overlay.cpp tray.cpp settings.cpp ^
    /link /SUBSYSTEM:WINDOWS ^
    User32.lib Shell32.lib Ole32.lib Gdi32.lib Advapi32.lib ^
    /OUT:.build\TaskbarVolume.exe
if errorlevel 1 exit /b 1
copy /y .build\TaskbarVolume.exe TaskbarVolume.exe >nul
if errorlevel 1 exit /b 1
