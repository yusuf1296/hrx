@echo off
REM Osiris manual-map injector derleme (Visual Studio 2022).
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >NUL
cl /nologo /std:c++17 /O2 /MD /EHsc /DNOMINMAX src\main.cpp /Fe:HRX.exe /link /SUBSYSTEM:CONSOLE /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
if errorlevel 1 (
  echo.
  echo DERLEME BASARISIZ
  exit /b 1
)
echo.
echo Cikti: HRX.exe
echo Kullanim: HRX.exe [Osiris.dll] [--wait] [--keep-headers]
