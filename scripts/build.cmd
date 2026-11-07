@rem =============================================================================
@rem build.cmd - Wrapper de Windows para scripts/build.sh (msys2 de devkitPro)
@rem =============================================================================
@rem Doble clic desde la raiz del proyecto: compila y arma dist/ para la SD.
@rem Argumentos: se pasan tal cual a build.sh (ej: build.cmd --fresh)

@echo off
setlocal

set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not exist "%BASH%" (
    if defined DEVKITPRO if exist "%DEVKITPRO%\msys2\usr\bin\bash.exe" (
        set "BASH=%DEVKITPRO%\msys2\usr\bin\bash.exe"
    ) else (
        echo ERROR: no se encuentra el bash de devkitPro.
        echo Instale devkitPro en C:\devkitPro o asigne la variable DEVKITPRO.
        exit /b 1
    )
)

rem paso del cwd hacia bash via env: bash hace cd "$(cygpath -u ...)"
set "REPO_CWD=%CD%"

"%BASH%" -lc "cd \"$(cygpath -u \"%REPO_CWD%\")\" && ./scripts/build.sh %*"
