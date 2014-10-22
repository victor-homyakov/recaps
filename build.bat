@echo off

set FILENAME=recaps-0.7

:: clean up
if exist Output\*.* del /q Output\*.*
if exist Release\nul rd /s/q Release
if exist src\nul rd /s/q src

:: build binaries
devenv recaps.sln /rebuild Release
if errorlevel 1 goto error

:: build binary distribution using Inno Setup
iscc /F%FILENAME%-setup setup.iss
if errorlevel 1 goto error

:: copy standalone exe
copy Release\recaps.exe Output\%FILENAME%.exe
if errorlevel 1 goto error

:: build source distribution using 7zip
mkdir src
copy *.* src
del src\*.ncb
del src\*.aps
del src\*.user
7z -tzip a Output\%FILENAME%-src.zip src/*
if errorlevel 1 goto error
rd /s/q src

goto end

:error
pause

:end
