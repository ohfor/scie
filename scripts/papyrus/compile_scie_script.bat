@echo off
REM SCIE Papyrus Script Compiler
REM Compiles all SCIE_*.psc scripts from source\ subfolder to the game Scripts folder
REM
REM Usage:
REM   compile_scie_script.bat              - Compile all SCIE scripts
REM   compile_scie_script.bat ScriptName   - Compile specific script (without .psc extension)
REM
REM After compiling, copy the .pex files back to the repo:
REM   copy /Y "%SKYRIM%\Data\Scripts\SCIE_*.pex" scripts\papyrus\compiled\

setlocal enabledelayedexpansion

REM === Configuration ===
set "SKYRIM=D:\SteamLibrary\steamapps\common\Skyrim Special Edition"
set "COMPILER=%SKYRIM%\Papyrus Compiler\PapyrusCompiler.exe"
set "FLAGS=%SKYRIM%\Data\source\Scripts\TESV_Papyrus_Flags.flg"
set "OUTPUT=%SKYRIM%\Data\Scripts"

REM Import paths: SKSE/SkyUI first (has extended functions), then vanilla, then our source folder
set "IMPORT=%SKYRIM%\Data\Scripts\Source;%SKYRIM%\Data\source\Scripts;%~dp0source"

REM === Validation ===
if not exist "%COMPILER%" (
    echo ERROR: PapyrusCompiler.exe not found at:
    echo   %COMPILER%
    echo.
    echo Make sure Skyrim SE is installed and Creation Kit files are present.
    if "%NOPAUSE%"=="" pause
    exit /b 1
)

if not exist "%FLAGS%" (
    echo ERROR: Flags file not found at:
    echo   %FLAGS%
    if "%NOPAUSE%"=="" pause
    exit /b 1
)

REM === Compile ===
echo.
echo SCIE Papyrus Compiler
echo =====================
echo Compiler: %COMPILER%
echo Output:   %OUTPUT%
echo.

if "%~1"=="" (
    REM Compile all SCIE scripts
    echo Compiling all SCIE_*.psc scripts...
    echo.

    set "COUNT=0"
    for %%f in ("%~dp0source\SCIE_*.psc") do (
        echo Compiling: %%~nxf
        "%COMPILER%" "%%f" -f="%FLAGS%" -i="%IMPORT%" -o="%OUTPUT%" -op
        if !errorlevel! neq 0 (
            echo   FAILED: %%~nxf
        ) else (
            echo   OK: %%~nf.pex
            set /a COUNT+=1
        )
        echo.
    )

    echo.
    echo Compiled !COUNT! script(s^) to: %OUTPUT%
) else (
    REM Compile specific script
    set "SCRIPT=%~dp0source\%~1.psc"

    if not exist "!SCRIPT!" (
        echo ERROR: Script not found: !SCRIPT!
        pause
        exit /b 1
    )

    echo Compiling: %~1.psc
    "%COMPILER%" "!SCRIPT!" -f="%FLAGS%" -i="%IMPORT%" -o="%OUTPUT%" -op

    if !errorlevel! neq 0 (
        echo FAILED to compile %~1.psc
        pause
        exit /b 1
    )

    echo.
    echo Compiled: %~1.pex
)

REM === Sanitize PII from compiled scripts ===
echo.
echo Sanitizing PII from compiled scripts...
python "%~dp0..\sanitize_pex.py" --dir "%OUTPUT%"
if !errorlevel! neq 0 (
    echo WARNING: PEX sanitization failed
)

echo.
echo To copy compiled scripts back to repo:
echo   copy /Y "%OUTPUT%\SCIE_*.pex" "%~dp0compiled\"
echo.

REM Only pause if running interactively (not from another script)
if "%NOPAUSE%"=="" pause
