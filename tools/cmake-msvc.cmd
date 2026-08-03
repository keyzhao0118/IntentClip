@echo off
setlocal

set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"

if not exist "%VSDEVCMD%" (
    echo Visual Studio developer environment was not found: 1>&2
    echo   %VSDEVCMD% 1>&2
    exit /b 1
)

call "%VSDEVCMD%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%

"C:\Program Files\CMake\bin\cmake.exe" %*
exit /b %errorlevel%
