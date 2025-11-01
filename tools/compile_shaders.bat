@echo off
set SCRIPT_DIR=%~dp0
set POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe
"%POWERSHELL%" -ExecutionPolicy Bypass -File "%SCRIPT_DIR%compile_shaders.ps1" %*
IF %ERRORLEVEL% NEQ0 EXIT /B %ERRORLEVEL%
EXIT /B0
