/*
** Configuration header.
** Copyright (C) 2005-2015 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef luaconf_h
#define luaconf_h

#ifndef WINVER
#define WINVER 0x0501
#endif
#include <limits.h>
#include <stddef.h>
#include "lj_arch.h"

/* Default path for loading Lua and C modules with require(). */
#if defined(_WIN32)
/*
** On Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define LUA_LDIR	"!\\lua\\"
#define LUA_CDIR	"!\\"
#define _LUA_PATH_DEFAULT \
  LUA_LDIR"?.lua;" LUA_LDIR"?\\init.lua;"
#define _LUA_PATH_CWD ".\\?.lua" 
#define _LUA_CPATH_DEFAULT \
  LUA_CDIR"?.dll;" LUA_CDIR"loadall.dll"
#define _LUA_CPATH_CWD ".\\?.dll" 
#else
/*
** Note to distribution maintainers: do NOT patch the following lines!
** Please read ../doc/install.html#distro and pass PREFIX=/usr instead.
*/
#ifndef LUA_MULTILIB
#define LUA_MULTILIB	"lib"
#endif
#ifndef LUA_LMULTILIB
#define LUA_LMULTILIB	"lib"
#endif
#define LUA_LROOT	"/usr/local"
#if defined(LUAJIT_ENABLE_LUA51COMPAT)
#define LUA_LUADIR	"/lua/5.1/"
#elif defined(LUAJIT_ENABLE_LUA53COMPAT)
#define LUA_LUADIR	"/lua/5.3/"
#else
#define LUA_LUADIR	"/lua/5.2/"
#endif
#define LUA_LJDIR	"/luajit-" LJX_VERSION "/"

#ifdef LUA_ROOT
#define LUA_JROOT	LUA_ROOT
#define LUA_RLDIR	LUA_ROOT "/share" LUA_LUADIR
#define LUA_RCDIR	LUA_ROOT "/" LUA_MULTILIB LUA_LUADIR
#define LUA_RLPATH	";" LUA_RLDIR "?.lua;" LUA_RLDIR "?/init.lua"
#define LUA_RCPATH	";" LUA_RCDIR "?.so;"
#else
#define LUA_JROOT	LUA_LROOT
#define LUA_RLPATH
#define LUA_RCPATH
#endif

#define LUA_JPATH	LUA_JROOT "/share" LUA_LJDIR "?.lua"
#define LUA_LLDIR	LUA_LROOT "/share" LUA_LUADIR
#define LUA_LCDIR	LUA_LROOT "/" LUA_LMULTILIB LUA_LUADIR
#define LUA_LLPATH	LUA_LLDIR "?.lua;" LUA_LLDIR "?/init.lua"
#define LUA_LCPATH1	LUA_LCDIR "?.so"
#define LUA_LCPATH2	LUA_LCDIR "loadall.so"

#define _LUA_PATH_DEFAULT	LUA_JPATH ";" LUA_LLPATH LUA_RLPATH
#define _LUA_PATH_CWD           "./?.lua" 
#define _LUA_CPATH_DEFAULT	LUA_LCPATH1 ";" LUA_RCPATH ";" LUA_LCPATH2
#define _LUA_CPATH_CWD          "./?.so" 
#endif

/* Order of current directory depends on Lua version. */
#if defined(LUAJIT_ENABLE_LUA51COMPAT)
#define LUA_PATH_DEFAULT _LUA_PATH_CWD ";" _LUA_PATH_DEFAULT
#define LUA_CPATH_DEFAULT _LUA_CPATH_CWD ";" _LUA_CPATH_DEFAULT
#else
#define LUA_PATH_DEFAULT _LUA_PATH_DEFAULT ";" _LUA_PATH_CWD
#define LUA_CPATH_DEFAULT _LUA_CPATH_DEFAULT ";" _LUA_CPATH_CWD
#endif

/* Environment variable names for path overrides and initialization code. */
#define LUA_PATH	"LUA_PATH"
#define LUA_CPATH	"LUA_CPATH"
#define LUA_INIT	"LUA_INIT"
#define LUA_INIT_5_2	"LUA_INIT_5_2"

/* Special file system characters. */
#if defined(_WIN32)
#define LUA_DIRSEP	"\\"
#else
#define LUA_DIRSEP	"/"
#endif
#define LUA_PATHSEP	";"
#define LUA_PATH_MARK	"?"
#define LUA_EXECDIR	"!"
#define LUA_IGMARK	"-"
#define LUA_PATH_CONFIG \
  LUA_DIRSEP "\n" LUA_PATHSEP "\n" LUA_PATH_MARK "\n" \
  LUA_EXECDIR "\n" LUA_IGMARK

/* Quoting in error messages. */
#define LUA_QL(x)	"'" x "'"
#define LUA_QS		LUA_QL("%s")

/* Various tunables. */
#define LUAI_MAXSTACK	65500	/* Max. # of stack slots for a thread (<64K). */
#define LUAI_MAXCSTACK	8000	/* Max. # of stack slots for a C func (<10K). */
#define LUAI_MAXCFRAME (1*1024*1024) /* Max C stack, between 0.5-8MB. */
#define LUAI_GCPAUSE	200	/* Pause GC until memory is at 200%. */
#define LUAI_GCMUL	200	/* Run GC at 200% of allocation speed. */
#define LUA_MAXCAPTURES	32	/* Max. pattern captures. */

/* Compatibility with older library function names. */
#define LUA_COMPAT_MOD		/* OLD: math.mod, NEW: math.fmod */
#define LUA_COMPAT_GFIND	/* OLD: string.gfind, NEW: string.gmatch */

/* Configuration for the frontend (the luajit executable). */
#if defined(luajit_c)
#define LUA_PROGNAME	"luajit"  /* Fallback frontend name. */
#define LUA_PROMPT	"> "	/* Interactive prompt. */
#define LUA_PROMPT2	">> "	/* Continuation prompt. */
#define LUA_MAXINPUT	512	/* Max. input line length. */
#endif

/* Note: changing the following defines breaks the ABI. */
#if LJ_ABIVER == 53
#define LUA_INTEGER long long
#define LUA_UNSIGNED unsigned long long
#else
#define LUA_INTEGER	ptrdiff_t
#define LUA_UNSIGNED    unsigned int
#endif

#define LUA_IDSIZE	60	/* Size of lua_Debug.short_src. */
/*
** Size of lauxlib and io.* on-stack buffers. Weird workaround to avoid using
** unreasonable amounts of stack space, but still retain ABI compatibility.
** Blame Lua for depending on BUFSIZ in the ABI, blame **** for wrecking it.
*/
#define LUAL_BUFFERSIZE	(BUFSIZ > 16384 ? 8192 : BUFSIZ)
#define LUA_KCONTEXT    ptrdiff_t
#define lua_str2number(s,p)	strtod((s), (p))


/* The following defines are here only for compatibility with luaconf.h
** from the standard Lua distribution. They must not be changed for LuaJIT.
*/
#define LUA_NUMBER_DOUBLE
#define LUA_NUMBER		double
#define LUAI_UACNUMBER		double
#define LUA_NUMBER_SCAN		"%lf"
#define LUA_NUMBER_FMT		"%.14g"
#define lua_number2str(s, n)	sprintf((s), LUA_NUMBER_FMT, (n))
#define LUAI_MAXNUMBER2STR	32
#define LUA_INTFRMLEN		"l"
#define LUA_INTFRM_T		long

/* Linkage of public API functions. */
#if !defined(LUA_API)
#if defined(LUA_BUILD_AS_DLL)
#if defined(LUA_CORE) || defined(LUA_LIB)
#define LUA_API		__declspec(dllexport)
#else
#define LUA_API		__declspec(dllimport)
#endif
#else
#define LUA_API		extern
#endif
#endif

#define LUALIB_API	LUA_API

/* Support for internal assertions. */
#if defined(LUA_USE_ASSERT) || defined(LUA_USE_APICHECK)
#include <assert.h>
#endif
#ifdef LUA_USE_ASSERT
#define lua_assert(x)		assert(x)
#endif
#ifdef LUA_USE_APICHECK
#define luai_apicheck(L, o)	{ (void)L; assert(o); }
#else
#define luai_apicheck(L, o)	{ (void)L; }
#endif

/* Define only if readline is available. */
#if LJ_TARGET_READLINE
#define LUA_USE_READLINE
#endif

#endif

