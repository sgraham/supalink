@echo off
:: Copyright 2011 Google Inc. All Rights Reserved.
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::     http:::www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.

setlocal

:: build our shim
cl /nologo /Ox /Zi /W4 /WX /D_CRT_SECURE_NO_WARNINGS /EHsc supalink.cpp /link /out:supalink.exe
if errorlevel 1 goto dead

:: == which link.exe
for %%i in (link.exe) do @set REALLINK=%%~$PATH:i

set SAVETO=%REALLINK%.supalink_orig.exe

if exist "%SAVETO%" goto alreadyexists

echo.
echo Going to save original link.exe from:
echo.
echo   %REALLINK%
echo.
echo as:
echo.
echo   %SAVETO%
echo.
echo (as well as the associated .config file) and replace link.exe with supalink.exe.
echo.
echo OK? (Ctrl-C to cancel)
pause

copy /y "%REALLINK%" "%SAVETO%"
copy /y "%REALLINK%.config" "%SAVETO%.config"
copy /y supalink.exe "%REALLINK%"
echo Done. You should be able to enable "Use Library Dependency Inputs" on large projects now.

goto done

:dead
echo Maybe run vsvars32.bat first?
goto done

:alreadyexists
echo Backup location "%SAVETO%" already exists. You probably want to restore that to link.exe first.
goto done

:done
