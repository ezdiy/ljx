/*
** $Id: lua.h,v 1.218.1.5 2008/08/06 13:30:12 roberto Exp $
** Lua - An Extensible Extension Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/


#ifndef lua_h
#define lua_h

#include <stdarg.h>
#include <stddef.h>


#include "luaconf.h"


#define LUA_VERSION_LJX         1
#define LUA_VERSION_MAJOR       "5"

#if LJ_51
#define LUA_VERSION_MINOR       "1"
#define LUA_VERSION_NUM         501
#elif LJ_53
#define LUA_VERSION_MINOR       "3"
#define LUA_VERSION_NUM         503
#else
#define LUA_VERSION_MINOR       "2"
#define LUA_VERSION_NUM         502
#endif

#if LJ_ABIVER==53
#define LUA_ABIVER_STRING       "5.3"
#elif LJ_ABIVER==52
#define LUA_ABIVER_STRING       "5.2"
#else
#define LUA_ABIVER_STRING       "5.1"
#endif

#define LUA_VERSION     "Lua " LUA_VERSION_MAJOR "." LUA_VERSION_MINOR
#define LUA_RELEASE     LUA_VERSION "." LUA_VERSION_RELEASE

#define LUA_COPYRIGHT	"Copyright (C) 1994-2013 Lua.org, PUC-Rio"
#define LUA_AUTHORS	"R. Ierusalimschy, L. H. de Figueiredo & W. Celes"
#define LUA_URL "http://lua.org/"


/* mark for precompiled code (`<esc>Lua') */
#define	LUA_SIGNATURE	"\033Lua"

/* option for multiple returns in `lua_pcall' and `lua_call' */
#define LUA_MULTRET	(-1)

/* Rio derives this from LUA_MAX_STACK */
#define RIO_LUAI_MAXSTACK		15000

/*
** XABI: pseudo-indices
** Those are carefuly laid out depending on ABI. Note that all indice
** types are supported (ie you can use 5.1 indices with 5.3 ABI), but
** then it will be only source compatible.
*/
#if LJ_ABIVER == 51
#define LUAI_FIRSTPSEUDOIDX	(-10000)
#define LUA_REGISTRYINDEX	(LUAI_FIRSTPSEUDOIDX)
#define LUA_ENVIRONINDEX	(LUAI_FIRSTPSEUDOIDX-1)
#define LUA_GLOBALSINDEX	(LUAI_FIRSTPSEUDOIDX-2)
#define LUA_UVINDEX             LUA_GLOBALSINDEX
#else /* 5.2 and 5.3 */
#define LUAI_FIRSTPSEUDOIDX	(-RIO_LUAI_MAXSTACK - 1000)
#define LUA_REGISTRYINDEX       (LUAI_FIRSTPSEUDOIDX)
#define LUA_ENVIRONINDEX        (LUAI_FIRSTPSEUDOIDX+1)
#define LUA_GLOBALSINDEX        (LUAI_FIRSTPSEUDOIDX+2)
#define LUA_UVINDEX             LUA_REGISTRYINDEX
#endif
#define lua_upvalueindex(i)	(LUA_UVINDEX-(i))

/* 5.2/3: predefined values in the registry */
#define LUA_RIDX_MAINTHREAD	1
#define LUA_RIDX_GLOBALS	2
#define LUA_RIDX_LAST		LUA_RIDX_GLOBALS
#define LUA_RIDX_USERVAL        3 /* For lua_setuservalue(). */
#define LUA_RIDX_COUNT          LUA_RIDX_USERVAL

/* thread status */
#define LUA_OK		0
#define LUA_YIELD	1
#define LUA_ERRRUN	2
#define LUA_ERRSYNTAX	3
#define LUA_ERRMEM	4
/* XABI */
#if LJ_ABIVER == 51
#define LUA_ERRERR	5
#else
#define LUA_ERRGCMM	5
#define LUA_ERRERR	6
#endif


typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State *L);
typedef LUA_KCONTEXT lua_KContext;
typedef int (*lua_KFunction) (lua_State *L, int status, lua_KContext ctx);


/*
** functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char * (*lua_Reader) (lua_State *L, void *ud, size_t *sz);

typedef int (*lua_Writer) (lua_State *L, const void* p, size_t sz, void* ud);


/*
** prototype for memory-allocation functions
*/
typedef void * (*lua_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** basic types
*/
#define LUA_TNONE		(-1)

#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8

#define LUA_NUMTAGS             9

/* minimum Lua stack available to a C function */
#define LUA_MINSTACK	20


/*
** generic extra include file
*/
#if defined(LUA_USER_H)
#include LUA_USER_H
#endif


/* type of numbers in Lua */
typedef LUA_NUMBER lua_Number;


/* type for integer functions */
typedef LUA_INTEGER lua_Integer;
typedef LUA_UNSIGNED lua_Unsigned;

/* unsigned integer type */
typedef unsigned long lua_Unsigned;


/*
** state manipulation
*/
LUA_API lua_State *(lua_newstate) (lua_Alloc f, void *ud);
LUA_API void       (lua_close) (lua_State *L);
LUA_API lua_State *(lua_newthread) (lua_State *L);

LUA_API lua_CFunction (lua_atpanic) (lua_State *L, lua_CFunction panicf);


/*
** basic stack manipulation
*/
LUA_API int   (lua_absindex) (lua_State *L, int idx);
LUA_API int   (lua_gettop) (lua_State *L);
LUA_API void  (lua_settop) (lua_State *L, int idx);
LUA_API void  (lua_rotate) (lua_State *L, int idx, int n);
LUA_API void  (lua_pushvalue) (lua_State *L, int idx);
LUA_API void  (lua_remove) (lua_State *L, int idx);
LUA_API void  (lua_insert) (lua_State *L, int idx);
LUA_API void  (lua_replace) (lua_State *L, int idx);
LUA_API void  (lua_copy) (lua_State *L, int fromidx, int toidx);
LUA_API int   (lua_checkstack) (lua_State *L, int sz);

LUA_API void  (lua_xmove) (lua_State *from, lua_State *to, int n);


/*
** access functions (stack -> C)
*/

LUA_API int             (lua_isnumber) (lua_State *L, int idx);
LUA_API int             (lua_isstring) (lua_State *L, int idx);
LUA_API int             (lua_iscfunction) (lua_State *L, int idx);
LUA_API int             (lua_isinteger) (lua_State *L, int idx);
LUA_API int             (lua_isuserdata) (lua_State *L, int idx);
LUA_API int             (lua_type) (lua_State *L, int idx);
LUA_API const char     *(lua_typename) (lua_State *L, int tp);

/*
** Comparison and arithmetic functions
*/
#define LUA_OPADD       0
#define LUA_OPSUB       1
#define LUA_OPMUL       2

#if LJ_ABIVER==53
#define LUA_OPMOD       3
#define LUA_OPPOW       4
#define LUA_OPDIV       5
#define LUA_OPIDIV      6
#define LUA_OPBAND      7
#define LUA_OPBOR       8
#define LUA_OPBXOR      9
#define LUA_OPSHL       10
#define LUA_OPSHR       11
#define LUA_OPUNM       12
#define LUA_OPBNOT      13
#else /* 5.2, 5.1 */
#define LUA_OPDIV       3
#define LUA_OPMOD       4
#define LUA_OPPOW       5
#define LUA_OPUNM       6
#define LUA_OPIDIV      7
#define LUA_OPBAND      8
#define LUA_OPBOR       9
#define LUA_OPBXOR      10
#define LUA_OPSHL       11
#define LUA_OPSHR       12
#define LUA_OPBNOT      13
#endif

LUA_API void  		(lua_arith) (lua_State *L, int op);

#define LUA_OPEQ        0
#define LUA_OPLT        1
#define LUA_OPLE        2

LUA_API int            (lua_equal) (lua_State *L, int idx1, int idx2);
LUA_API int            (lua_rawequal) (lua_State *L, int idx1, int idx2);
LUA_API int            (lua_compare) (lua_State *L, int index1, int index2, int op);
LUA_API int            (lua_lessthan) (lua_State *L, int idx1, int idx2);

LUA_API lua_Number      (lua_tonumber) (lua_State *L, int idx);
LUA_API lua_Number      (lua_tonumberx) (lua_State *L, int idx, int *succ);
LUA_API lua_Integer     (lua_tointeger) (lua_State *L, int idx);
LUA_API lua_Integer     (lua_tointegerx) (lua_State *L, int idx, int *succ);
LUA_API lua_Unsigned    (lua_tounsigned) (lua_State *L, int idx);
LUA_API lua_Unsigned    (lua_tounsignedx) (lua_State *L, int idx, int *succ);

LUA_API int             (lua_toboolean) (lua_State *L, int idx);
LUA_API const char     *(lua_tolstring) (lua_State *L, int idx, size_t *len);
LUA_API void         (lua_len) (lua_State *L, int i);
LUA_API size_t          (lua_objlen) (lua_State *L, int idx);
LUA_API size_t          (lua_rawlen) (lua_State *L, int idx);
LUA_API lua_CFunction   (lua_tocfunction) (lua_State *L, int idx);
LUA_API void	       *(lua_touserdata) (lua_State *L, int idx);
LUA_API lua_State      *(lua_tothread) (lua_State *L, int idx);
LUA_API const void     *(lua_topointer) (lua_State *L, int idx);
LUA_API const lua_Number *(lua_version) (lua_State *L);


/*
** push functions (C -> stack)
*/
LUA_API void  (lua_pushnil) (lua_State *L);
LUA_API void  (lua_pushnumber) (lua_State *L, lua_Number n);
LUA_API void  (lua_pushinteger) (lua_State *L, lua_Integer n);
LUA_API void  (lua_pushunsigned) (lua_State *L, lua_Unsigned n);
LUA_API void  (lua_pushlstring) (lua_State *L, const char *s, size_t l);
LUA_API void  (lua_pushstring) (lua_State *L, const char *s);
LUA_API const char *(lua_pushvfstring) (lua_State *L, const char *fmt,
                                                      va_list argp);
LUA_API const char *(lua_pushfstring) (lua_State *L, const char *fmt, ...);
LUA_API void  (lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n);
LUA_API void  (lua_pushboolean) (lua_State *L, int b);
LUA_API void  (lua_pushlightuserdata) (lua_State *L, void *p);
LUA_API void  (lua_pushuserdata_native) (lua_State *L, void *p);
#define lua_pushuserdata lua_pushuserdata_native
LUA_API int   (lua_pushthread) (lua_State *L);


/*
** get functions (Lua -> stack)
*/
LUA_API int   (lua_gettable) (lua_State *L, int idx);
LUA_API int   (lua_getfield) (lua_State *L, int idx, const char *k);
LUA_API int   (lua_rawget) (lua_State *L, int idx);
LUA_API int   (lua_geti) (lua_State *L, int idx, lua_Integer n);
LUA_API int   (lua_rawgeti) (lua_State *L, int idx, int n);
LUA_API int   (lua_rawgetp) (lua_State *L, int idx, const void *);

LUA_API void  (lua_createtable) (lua_State *L, int narr, int nrec);
LUA_API void *(lua_newuserdata) (lua_State *L, size_t sz);
LUA_API int   (lua_getmetatable) (lua_State *L, int objindex);
LUA_API void  (lua_getfenv) (lua_State *L, int idx);
LUA_API int   (lua_getuservalue) (lua_State *L, int idx);


/*
** set functions (stack -> Lua)
*/
LUA_API void  (lua_settable) (lua_State *L, int idx);
LUA_API void  (lua_setfield) (lua_State *L, int idx, const char *k);
LUA_API void  (lua_rawset) (lua_State *L, int idx);
LUA_API void  (lua_rawseti) (lua_State *L, int idx, int n);
LUA_API void  (lua_rawsetp) (lua_State *L, int idx, const void *p);
LUA_API int   (lua_setmetatable) (lua_State *L, int objindex);
LUA_API int   (lua_setfenv) (lua_State *L, int idx);
LUA_API void  (lua_setuservalue) (lua_State *L, int idx);
#if LJ_ABIVER==51
/* Some users check for those macros */
#define lua_setglobal(L,s)	lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define lua_getglobal(L,s)	lua_getfield(L, LUA_GLOBALSINDEX, (s))
#else
LUA_API void (lua_setglobal) (lua_State *L, const char *var);
LUA_API int (lua_getglobal) (lua_State *L, const char *var);
#endif



/*
** `load' and `call' functions (load and run Lua code)
*/
LUA_API void  (lua_callk) (lua_State *L, int nargs, int nresults,
		                           lua_KContext ctx, lua_KFunction k);
LUA_API int   (lua_pcallk) (lua_State *L, int nargs, int nresults, int errfunc,
		                            lua_KContext ctx, lua_KFunction k);
LUA_API void  (lua_call) (lua_State *L, int nargs, int nresults);
LUA_API int   (lua_pcall) (lua_State *L, int nargs, int nresults, int errfunc);
LUA_API int   (lua_cpcall) (lua_State *L, lua_CFunction func, void *ud);
LUA_API int   (lua_load) (lua_State *L, lua_Reader reader, void *data,
		     const char *chunkname
#if LJ_ABIVER!=51
		     , const char *mode
#endif
		     );
LUA_API int (lua_dump) (lua_State *L, lua_Writer writer, void *data);


/*
** coroutine functions
*/
LUA_API int  (lua_yieldk)     (lua_State *L, int nresults, lua_KContext ctx,
                               lua_KFunction k);
LUA_API int  (lua_yield) (lua_State *L, int nresults);
LUA_API int  (lua_resume) (lua_State *L, int narg);
LUA_API int  (lua_status) (lua_State *L);
LUA_API int (lua_isyieldable) (lua_State *L);

/*
** garbage-collection function and options
*/

#define LUA_GCSTOP		0
#define LUA_GCRESTART		1
#define LUA_GCCOLLECT		2
#define LUA_GCCOUNT		3
#define LUA_GCCOUNTB		4
#define LUA_GCSTEP		5
#define LUA_GCSETPAUSE		6
#define LUA_GCSETSTEPMUL	7
#define LUA_GCSETMAJORINC       8
#define LUA_GCISRUNNING         9
#define LUA_GCGEN               10
#define LUA_GCINC               11

LUA_API int (lua_gc) (lua_State *L, int what, int data);


/*
** miscellaneous functions
*/

LUA_API int   (lua_error) (lua_State *L);

LUA_API int   (lua_next) (lua_State *L, int idx);

LUA_API void  (lua_concat) (lua_State *L, int n);

LUA_API size_t (lua_stringtonumber) (lua_State *L, const char *s);
LUA_API lua_Alloc (lua_getallocf) (lua_State *L, void **ud);
LUA_API void lua_setallocf (lua_State *L, lua_Alloc f, void *ud);

LUA_API void lua_setexdata(lua_State *L, void *exdata);
LUA_API void *lua_getexdata(lua_State *L);


/*
** ===============================================================
** some useful macros
** ===============================================================
*/

#define lua_pop(L,n)		lua_settop(L, -(n)-1)

#define lua_newtable(L)		lua_createtable(L, 0, 0)

#define lua_register(L,n,f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))

#define lua_pushcfunction(L,f)	lua_pushcclosure(L, (f), 0)

#define lua_strlen(L,i)		lua_rawlen(L, (i))

#define lua_isfunction(L,n)	(lua_type(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L,n)	(lua_type(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L,n)	(lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)		(lua_type(L, (n)) == LUA_TNIL)
#define lua_isboolean(L,n)	(lua_type(L, (n)) == LUA_TBOOLEAN)
#define lua_isthread(L,n)	(lua_type(L, (n)) == LUA_TTHREAD)
#define lua_isnone(L,n)		(lua_type(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L, n)	(lua_type(L, (n)) <= 0)

#define lua_pushliteral(L, s)	\
	lua_pushlstring(L, "" s, (sizeof(s)/sizeof(char))-1)

#define lua_tostring(L,i)	lua_tolstring(L, (i), NULL)

#define lua_pushglobaltable(L)  \
        lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS)

/*
** compatibility macros and functions
*/

#define lua_open()	luaL_newstate()

#define lua_getregistry(L)	lua_pushvalue(L, LUA_REGISTRYINDEX)

#define lua_getgccount(L)	lua_gc(L, LUA_GCCOUNT, 0)

#define lua_Chunkreader		lua_Reader
#define lua_Chunkwriter		lua_Writer


/* hack */
LUA_API void lua_setlevel	(lua_State *from, lua_State *to);


/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define LUA_HOOKCALL	0
#define LUA_HOOKRET	1
#define LUA_HOOKLINE	2
#define LUA_HOOKCOUNT	3
#define LUA_HOOKTAILRET 4
#define LUA_HOOKTAILCALL 4


/*
** Event masks
*/
#define LUA_MASKCALL	(1 << LUA_HOOKCALL)
#define LUA_MASKRET	(1 << LUA_HOOKRET)
#define LUA_MASKLINE	(1 << LUA_HOOKLINE)
#define LUA_MASKCOUNT	(1 << LUA_HOOKCOUNT)

typedef struct lua_Debug lua_Debug;  /* activation record */


/* Functions to be called by the debuger in specific events */
typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar);
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar);
LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n);
LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n);
LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n);
LUA_API int lua_sethook (lua_State *L, lua_Hook func, int mask, int count);
LUA_API lua_Hook lua_gethook (lua_State *L);
LUA_API int lua_gethookmask (lua_State *L);
LUA_API int lua_gethookcount (lua_State *L);

/* From Lua 5.2. */
LUA_API void *lua_upvalueid (lua_State *L, int idx, int n);
LUA_API void lua_upvaluejoin (lua_State *L, int idx1, int n1, int idx2, int n2);
LUA_API int lua_loadx (lua_State *L, lua_Reader reader, void *dt,
		       const char *chunkname, const char *mode);
LUA_API const lua_Number *lua_version (lua_State *L);
LUA_API void lua_copy (lua_State *L, int fromidx, int toidx);
LUA_API lua_Number lua_tonumberx (lua_State *L, int idx, int *isnum);
LUA_API lua_Integer lua_tointegerx (lua_State *L, int idx, int *isnum);
LUA_API size_t lua_rawlen (lua_State *L, int idx);
LUA_API void lua_len (lua_State *L, int idx);
LUA_API int lua_absindex (lua_State *L, int idx);
LUA_API void luaL_requiref (lua_State *L, char const* modname, lua_CFunction openf, int glb);

/* From Lua 5.3. */
LUA_API int lua_isyieldable (lua_State *L);


struct lua_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) `global', `local', `field', `method' */
  const char *what;	/* (S) `Lua', `C', `main', `tail' */
  const char *source;	/* (S) */
#if LJ_ABIVER==51
  int currentline;	/* (l) */
  int nups;		/* (u) number of upvalues */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  char short_src[LUA_IDSIZE]; /* (S) */
  int i_ci;             /* active function */
#else
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  char istailcall;	/* (t) */
  char short_src[LUA_IDSIZE]; /* (S) */
  /* private part */
  int i_ci;  /* active function */
#endif
};

/* }====================================================================== */


/******************************************************************************
* Copyright (C) 1994-2008 Lua.org, PUC-Rio.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/


#endif
