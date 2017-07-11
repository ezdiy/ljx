@set LJLINK=link /nologo
@set LJCOMPILE=cl /nologo /c /O2 /W3 /D_CRT_SECURE_NO_DEPRECATE
@set LJLIB=lib /nologo /nodefaultlib
@set DASMDIR=..\dynasm
@set DASM=%DASMDIR%\dynasm.lua
@set LJLIBNAME=lua51.lib
@set ALL_LIB=lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c

%LJCOMPILE% host\minilua.c || exit /b 1
%LJLINK% /out:minilua.exe minilua.obj

@set DASMFLAGS=-D WIN -D JIT -D FFI
@set LJARCH=x86
@set LJCOMPILE=%LJCOMPILE% /arch:SSE2

minilua %DASM% -LN %DASMFLAGS% -o host\buildvm_arch.h vm_x86.dasc || exit /b 2
%LJCOMPILE% /I "." /I %DASMDIR% host\buildvm*.c || exit /b 3
%LJLINK% /out:buildvm.exe buildvm*.obj || exit /b 4

rem %LJMT% -manifest buildvm.exe.manifest -outputresource:buildvm.exe
buildvm -m peobj -o lj_vm.obj || exit /b 5
buildvm -m bcdef -o lj_bcdef.h %ALL_LIB% || exit /b 6
buildvm -m ffdef -o lj_ffdef.h %ALL_LIB% || exit /b 7
buildvm -m libdef -o lj_libdef.h %ALL_LIB% || exit /b 8
buildvm -m recdef -o lj_recdef.h %ALL_LIB% || exit /b 9
buildvm -m vmdef -o jit\vmdef.lua %ALL_LIB% || exit /b 10
buildvm -m folddef -o lj_folddef.h lj_opt_fold.c || exit /b 11

