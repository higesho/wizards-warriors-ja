@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=obj\release
set OUT=dinput.dll
set DEF=
set LIBS=kernel32.lib
if "%1"=="test" (
  set O=obj\test
  set OUT=test\dinput.dll
  set DEF=/DWWJP_TEST
  set LIBS=kernel32.lib user32.lib
)
if not exist %O% mkdir %O%
if "%1"=="test" if not exist test mkdir test
rc /nologo /fo %O%\wwjp.res wwjp.rc || exit /b 1
cl /nologo /O2 /W3 /MT /LD /utf-8 %DEF% /Fo%O%\ wwjp.c %O%\wwjp.res /Fe:%OUT% /link /DEF:dinput.def %LIBS% || exit /b 1
