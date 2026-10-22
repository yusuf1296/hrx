@echo off
rem HRX otomatik yayin - bu dosyaya cift tikla.
rem once: osiris\version.txt icindeki surumu artir, Osiris.sln Release x64 derle,
rem       injector icin osiris-injector\build.bat calistir (CIKTI: HRX.exe).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0YAYINLA.ps1"
set RC=%ERRORLEVEL%
echo.
if not "%RC%"=="0" echo YAYIN BASARISIZ (hata kodu %RC%)
pause
exit /b %RC%