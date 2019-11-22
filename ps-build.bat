@echo off

REM This is a convenience script written to make building PrusaSlicer on Windows easier
REM This script will only be maintained for the version of the toolset the main project supports. 

REM This script expects to be executed from "Native Tools Command Prompt for VS 2019"
REM This script will automatically set the CMAKE_GENERATOR based on the Platform variable 
REM set by "Native Tools Command Prompt for VS 2019"

REM The script expects the DEPS_DIR set below to exist or for it to be able to create it if not.

REM By default this script will warn you and pause if it is going to make changes to your filesystem
REM You can disable this by setting DO_WARN to false

REM TODO: make dependiency checks actually confirm dependiencies built successfully

REM ======== USER VARIABLES ======== 

REM USER: Set where you want the deps built or where extracted to
set DEPS_DIR=C:\local\dev\PrusaSlicer-deps

REM USER: Set PrusaSlicer configuration to buiild
REM set BUILD_CFG=Debug
REM set BUILD_CFG=MinSizeRel
REM set BUILD_CFG=RelWithDebInfo
REM set BUILD_CFG=Release
set BUILD_CFG=Release

REM USER: Choose if prerequisites cmake/built as necessary 
REM (this will not check to see if they are up to date)
set DO_PREREQS=true

REM USER: Enable or disable warnings
set DO_WARN=true

REM ======= SETUP ===========

REM Set path for provided deps dir
set PREFIX_PATH=%DEPS_DIR%\usr\local\

REM Confirm to make sure the deps dir exists, fail if it cannot be created 
if not exist "%DEPS_DIR%" (
    echo Dependencies directory does not exist.
    echo DEPS_DIR=%DEPS_DIR%
    echo Will attempt to make it.
    pause
    mkdir "%DEPS_DIR%"
    if not exist "%DEPS_DIR%" (
        echo Unable to create dependencies directory. 
        echo ps-build cannot continue. 
        exit /B 1 
    )
)

REM ======== CHECK EXPECTED ENVIRONMENT VARIABLES ======== 

REM Check for expected environemnt variables "VisualStudioVersion" and "VSCMD_ARG_TGT_ARCH" 
if not DEFINED VisualStudioVersion  ( 
    echo This script must be called from the Visual Studio Native Tools Command Prompt
    echo ps-build cannot continue. 
    exit /B 1
) 
if not DEFINED VSCMD_ARG_TGT_ARCH (
    echo This script must be called from the Visual Studio Native Tools Command Prompt
    echo ps-build cannot continue. 
    exit /B 1
)

REM =========== WRAP REMAINING SCRIPT FOR ERROR HANDLING =========

REM execute remainder of script in new CMD session
REM This is so we can ensure we always return to original directory 
REM in case user terminates script while in a subdirectory
if "%~1" neq "_start_" (
    REM Save the current working directory so we can return to it when the script exits
    SET CURRENT_DIR="%cd%"
    REM execute script
    cmd /c "%~f0" _start_ %*
    REM return to the original working directory before exiting
    cd %CURRENT_DIR%
    exit /b
)
shift /1

REM ========= SET CMAKE GENERATOR FROM TGT ARCH ===========

REM For VS2019 this requires setting the CMAKE_GEN_PLATFORM with -A x64
set CMAKE_GEN=
if %VSCMD_ARG_TGT_ARCH% == x64 (
    set CMAKE_GEN="Visual Studio 16" -A x64
) else (
    set CMAKE_GEN="Visual Studio 16"
)

REM ====== OUTPUT HEADER =======

echo.
echo PrusaSlicer Windows build script. 
echo NOTE: please ensure you edit ps-build.bat and update the user variables 
echo ====== USER VARIABLES ======
echo Dependencies Build Directory=%DEPS_DIR%
echo PrusaSlicer Build Config=%BUILD_CFG%
echo Build Prerequisites=%DO_PREREQS%
echo Warn when cleaning=%DO_WARN%
echo. 

REM ========= BASIC ARGUMENT PARSER ==========

if "%1" == "build" (
    REM === BUILD ARG SUPPLIED ===
    if "%2" == "deps" (
            echo Building PrusaSlicer Dependencies
            echo.
            pause
            goto :build_deps
    ) else (
        if "%2" == "ps" (
            echo Building PrusaSlicer 
            echo.
            pause
            goto :build_ps
        ) else (
            if "%2" == "all" (
                echo Building PrusaSlicer and Dependencies
                echo.
                pause
                goto :build_all
            )
        )
    )
) else (
    if "%1" == "clean" (
        REM === CLEAN ARG SUPPLIED == 
        if "%2" == "deps" (
            echo.
            echo Cleaning Dependencies
            echo.
            goto :clean_deps
        ) else (
            if "%2" == "ps" (
                echo.
                echo Cleaning PrusaSlicer
                echo.
                goto :clean_ps
            ) else (
                if "%2" == "all" (
                    echo.
                    echo Cleaning Dependencies and PrusaSlicer
                    echo.
                    goto :clean_all
                )
            )
        )
    ) else (
        if "%1" == "cmake" (
            REM === CMAKE ARG SUPPLIED == 
            if "%2" == "deps" (
                echo Executing cmake for dependencies
                echo.
                pause
                goto :cmake_deps
            ) else (
                if "%2" == "ps" (
                    echo Executing cmake for PrusaSlicer
                    echo.
                    pause
                    goto :cmake_ps
                ) else (
                    if "%2" == "all" (
                        echo Executing cmake for PrusaSlicer and dependencies
                        echo.
                        pause
                        goto :cmake_all
                    )
                )
            )
        )    
    )
)

REM unknown command
echo unknown command
goto :help

REM ======== USAGE ==========
:help
echo. 
echo usage: ps-build.bat [command] [target]
echo commands: build, clean, cmake
echo targets: deps, ps, all
echo.
exit /B 0

REM ============ BUILD FUNCITONS ============

REM ==== BUILD PrusaSlicer DEPS FUNCTION ====
:build_deps

    echo.

    REM run cmake_deps if it hasn't already
    if not exist deps\build\CMakeCache.txt (   
        echo cmake doesn't appear to have been run for dependencies.
        if "%DO_PREREQS%"=="true" (
            echo.
            echo Will run cmake for dependencies.
            echo.
            if "%DO_WARN%"=="true" (
                pause
            )
            call :cmake_deps
            if errorlevel 1 (
                echo Dependencies build failed 
                exit /B 1
            )
        ) else (
            echo ps-build is configured not to automatically run cmake
            echo ps-build cannot continue
            exit /B 1
        )
    ) 

    cd deps\build
   
    echo.
    echo Building Dependencies
    echo.

    REM execute MSBuild on the ALL_BUILD vc project (generated by cmake). 
    MSBuild.exe /m ALL_BUILD.vcxproj

    REM check to see if MSBuild returned an error
    if errorlevel 1 (
        echo Dependencies build failed 
        exit /B 1
    )

    echo.
    echo Dependencies built successfully
    echo.

    cd ..\..

exit /B 0

REM ==== BUILD PrusaSlicer FUNCTION ====
:build_ps

    echo.
    REM check for dependencies
    if not exist "%PREFIX_PATH%" (
        echo Dependencies don't appear to have been built.
        if "%DO_PREREQS%"=="true" (
            echo.
            echo Will run build dependencies.
            echo.
            if "%DO_WARN%"=="true" (
                pause
            )
            call :build_deps
            if errorlevel 1 (
                echo Issue building dependencies. 
                echo PrusaSlicer build failed.
                echo NOTE: You will need to resolve dependencies build issues
                echo       before building PrusaSlicer will succeed.
                echo       use: ps-build.bat build deps to try to build deps again
                exit /B 1
            )
        ) else (
            echo ps-build is configured not to automatically build prerequisites
            echo ps-build cannot continue
            exit /B 1
        )
    )

    REM run cmake_ps if it hasn't already
    if not exist build\CMakeCache.txt (   
        echo cmake doesn't appear to have been run for PS.
        if "%DO_PREREQS%"=="true" (
            echo.
            echo Will run run cmake for PrusaSlicer.
            echo.
            if "%DO_WARN%"=="true" (
                pause
            )
            call :cmake_ps
            if errorlevel 1 (
                echo PrusaSlicer build failed.
                echo NOTE: You will need to resolve the PrusaSlicer cmake issues
                echo       before building PrusaSlicer will succeed.
                echo       use: ps-build.bat cmake PrusaSlicer to try cmake again
                echo       Once cmake succeeds try ps-build build PrusaSlicer again
                exit /B 1
            )
        ) else (
            echo ps-build is configured not to automatically run cmake
            echo ps-build cannot continue
            exit /B 1
        )
    )

    cd build

    echo.
    echo Building PrusaSlicer
    echo.

    REM attempt to build PrusaSlicer using generated project file
    MSBuild.exe /m ALL_BUILD.vcxproj /p:Configuration=%BUILD_CFG%

    REM check to see if MSBuild returned an error
    if errorlevel 1 (
        echo PrusaSlicer Build Failed
        cd .. 
        exit /B 1
    )

    echo.
    echo PrusaSlicer built successfully
    echo.

    cd .. 

exit /B 0

REM ==== BUILD ALL FUNCTION ====
:build_all
    call :build_deps
    if errorlevel 1 (
        echo.
        echo build all failed
        echo.
        exit /B 1
    )
    call :build_ps
    if errorlevel 1 (
        echo.
        echo build all failed
        echo.
        exit /B 1
    )

exit /B 0

REM ============ CLEAN FUNCTIONS ============

REM ==== CLEAN PrusaSlicer DEPS FUNCTION ====

:clean_deps

    if "%DO_WARN%"=="true" (
        echo.
        echo WARNING: THIS WILL DELETE ALL DEPENDENCY BUILD FILES
        echo          THIS WILL REQUIRE A COMPLETE REBUILD OF DEPENDENCIES
        echo          deleting: deps\build
        echo          deleting: %DEPS_DIR%\usr
        echo.
        pause
    )

    del /s /q deps\build
    rmdir /s /q deps\build

    del /s /q "%DEPS_DIR%\usr"
    rmdir /s /q "%DEPS_DIR%\usr"

exit /B 0

REM ==== CLEAN PrusaSlicer FUNCTION ====
:clean_ps

    if "%DO_WARN%"=="true" (
        echo.
        echo WARNING: THIS WILL DELETE ALL PrusaSlicer BUILD FILES
        echo          THIS WILL REQUIRE A COMPLETE REBUILD OF PS
        echo          deleting: build
        echo.
        pause
    )

    del /s /q build
    rmdir /s /q build

exit /B 0

REM ==== CLEAN ALL FUNCTION ====
:clean_all

    call :clean_deps
    call :clean_ps

exit /B 0

REM ============ CMAKE FUNCTIONS ============

REM ==== CMAKE PrusaSlicer DEPS FUNCTION ====
:cmake_deps

    echo.

    cd deps
    REM create the build directory if it doesn't exist
    if not exist build (
        mkdir build 
    )
    cd build

    echo.
    echo Executing cmake for Dependencies
    echo.

    REM execute cmake with the GENERATOR we prepared earlier 
    cmake .. -G %CMAKE_GEN% -DDESTDIR='%DEPS_DIR%'

    REM check to see if cmake returned an error
    if errorlevel 1 (
        echo.
        echo CMAKE Executition Failed
        echo.
        exit /B 1
    )
    cd ..\.. 
    
    echo.
    echo cmake for dependencies succeeded
    echo.

exit /B 0

REM ==== CMAKE PrusaSlicer FUNCTION ====
:cmake_ps

    echo.

    REM create the build directory if it doesn't exist
    if not exist build (
        mkdir build 
    )
    cd build
        
    echo.
    echo Executing cmake for PrusaSlicer
    echo.

    REM execute cmake with the GENERATOR we prepared earlier 
    cmake .. -G %CMAKE_GEN% -DCMAKE_PREFIX_PATH='%PREFIX_PATH%'

    REM check to see if cmake returned an error
    if errorlevel 1 (
        echo.
        echo CMAKE Executition Failed
        echo.
        exit /B 1
    )

    cd ..

    echo.
    echo cmake for PrusaSlicer succeeded
    echo.

exit /B 0

REM ==== CMAKE ALL FUNCTION ====
:cmake_all
    call :cmake_deps
    if errorlevel 1 (
        echo.
        echo cmake all failed
        echo.
        exit /B 1
    )
    call :cmake_ps
    if errorlevel 1 (
        echo.
        echo cmake all failed
        echo.
        exit /B 1
    )
exit /B 0
