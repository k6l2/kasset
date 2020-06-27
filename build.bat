@echo off
SETLOCAL
rem --- Iterate over build script arguments ---
set buildOptionClean=FALSE
set buildOptionRelease=FALSE
if "%~1"=="" goto ARGUMENT_LOOP_END
rem set argNumber=0
:ARGUMENT_LOOP_START
rem echo arg%argNumber%=%1
if "%~1"=="release" (
	set buildOptionRelease=TRUE
)
if "%~1"=="clean" (
	set buildOptionClean=TRUE
)
shift
rem set /A argNumber+=1
if not "%~1"=="" goto ARGUMENT_LOOP_START
:ARGUMENT_LOOP_END
rem --- CLEAN build command ---
if "%buildOptionClean%"=="TRUE" (
	echo Cleaning all build files...
	del /S /F /Q "%project_root%\build" > NUL 2> NUL
	rmdir /S /Q "%project_root%\build" > NUL 2> NUL
	rmdir /S /Q "%project_root%\build" > NUL 2> NUL
	echo Clean complete!
	exit /B 0
)
rem --- setup build variables ---
set project_root=%~dp0
set exe_name=kasset
set CommonCompilerFlags=/wd4201 /wd4514 /wd4505 /wd4100 /wd5045 ^
	/Oi /GR- /EHsc /Zi /FC /nologo /std:c++latest
set CommonCompilerFlagsRelease=%CommonCompilerFlags% /O2 /MT /w /wd4711 ^
	/DINTERNAL_BUILD=0 /DSLOW_BUILD=0 
set CommonCompilerFlagsDebug=%CommonCompilerFlags% /MTd /Od /WX ^
	/DINTERNAL_BUILD=1 /DSLOW_BUILD=1 
set CommonCompilerFlagsChosen=%CommonCompilerFlagsDebug%
if "%buildOptionRelease%"=="TRUE" (
	set CommonCompilerFlagsChosen=%CommonCompilerFlagsRelease%
)
set CommonLinkerFlags=/opt:ref /incremental:no 
pushd %project_root%
if not exist build (
	mkdir build
)
rem --- Create a text tree of the code so we can skip the build if nothing 
rem     changed ---
pushd %project_root%\code
FOR /F "delims=" %%G IN ('DIR /B /S') DO (
	>>"%project_root%\build\code-tree-current.txt" ECHO %%~G,%%~tG,%%~zG
)
rem pop from %project_root%\code
popd
pushd build
rem --- Detect if the code tree differs, and if it doesn't, skip building ---
set codeTreeIsDifferent=FALSE
fc code-tree-existing.txt code-tree-current.txt > NUL 2> NUL
if %ERRORLEVEL% GTR 0 (
	set codeTreeIsDifferent=TRUE
)
del code-tree-existing.txt
ren code-tree-current.txt code-tree-existing.txt
IF "%codeTreeIsDifferent%"=="TRUE" (
	echo %exe_name% code tree has changed!  Continuing build...
) ELSE (
	echo %exe_name% code tree is unchanged!  Skipping build...
	GOTO :SKIP_BUILD
)
rem --- Build the executable ---
cl %project_root%\code\main.cpp /Fe%exe_name% %CommonCompilerFlagsChosen% ^
	/link %CommonLinkerFlags%
:SKIP_BUILD
rem pop from build
popd
rem pop from %project_root%
popd
ENDLOCAL