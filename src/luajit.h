/*
** LuaJIT -- a Just-In-Time Compiler for Lua. http://luajit.org/
**
** Copyright (C) 2005-2020 Mike Pall. All rights reserved.
**
** LJX - a bastardization of LuaJIT
** Copyright (C) 2014-2016 Karel Tuma, kat@lua.cz
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
**/

#ifndef _LUAJIT_H
#define _LUAJIT_H

#define MIT_LEGALESE \
"Permission is hereby granted, free of charge, to any person obtaining a copy\n" \
"of this software and associated documentation files (the \"Software\"), to deal\n" \
"in the Software without restriction, including without limitation the rights\n" \
"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n" \
"copies of the Software, and to permit persons to whom the Software is\n" \
"furnished to do so, subject to the following conditions:\n" \
"\n" \
"The above copyright notice and this permission notice shall be included in\n" \
"all copies or substantial portions of the Software.\n" \
"\n" \
"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n" \
"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n" \
"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE\n" \
"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n" \
"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n" \
"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n" \
"THE SOFTWARE.\n"


#include "lua.h"

#define LUAJIT_VERSION		"LuaJIT 2.1.0-beta3"
#define LJ_LJX_VERSION          LUAJIT_VERSION "+" LJX_VERSION

#define LUAJIT_VERSION_NUM	20100  /* Version 2.1.0 = 02.01.00. */
#define LUAJIT_VERSION_SYM	luaJIT_version_2_1_0_beta3
#define LUAJIT_COPYRIGHT	"Copyright (C) 2005-2020 Mike Pall"
#define LUAJIT_URL		"http://luajit.org/"

/* Let it be known who is responsible for this bastardization */
#define LJX_COPYRIGHT	"Copyright (C) 2014-2016 Karel Tuma"
#define LJX_URL		"http://lua.cz/"

#define LUA_COPYRIGHTS  \
  LUA_VERSION " -- " LUA_COPYRIGHT " -- " LUA_URL "\n" \
  LUAJIT_VERSION " -- " LUAJIT_COPYRIGHT " -- " LUAJIT_URL "\n" \
  LJX_VERSION " -- " LJX_COPYRIGHT " -- " LJX_URL "\n\n" \
  MIT_LEGALESE


/* Modes for luaJIT_setmode. */
#define LUAJIT_MODE_MASK	0x00ff

enum {
  LUAJIT_MODE_ENGINE,		/* Set mode for whole JIT engine. */
  LUAJIT_MODE_DEBUG,		/* Set debug mode (idx = level). */

  LUAJIT_MODE_FUNC,		/* Change mode for a function. */
  LUAJIT_MODE_ALLFUNC,		/* Recurse into subroutine protos. */
  LUAJIT_MODE_ALLSUBFUNC,	/* Change only the subroutines. */

  LUAJIT_MODE_TRACE,		/* Flush a compiled trace. */

  LUAJIT_MODE_WRAPCFUNC = 0x10,	/* Set wrapper mode for C function calls. */

  LUAJIT_MODE_MAX
};

/* Flags or'ed in to the mode. */
#define LUAJIT_MODE_OFF		0x0000	/* Turn feature off. */
#define LUAJIT_MODE_ON		0x0100	/* Turn feature on. */
#define LUAJIT_MODE_FLUSH	0x0200	/* Flush JIT-compiled code. */

/* LuaJIT public C API. */

/* Control the JIT engine. */
LUA_API int luaJIT_setmode(lua_State *L, int idx, int mode);

/* Low-overhead profiling API. */
typedef void (*luaJIT_profile_callback)(void *data, lua_State *L,
					int samples, int vmstate);
LUA_API void luaJIT_profile_start(lua_State *L, const char *mode,
				  luaJIT_profile_callback cb, void *data);
LUA_API void luaJIT_profile_stop(lua_State *L);
LUA_API const char *luaJIT_profile_dumpstack(lua_State *L, const char *fmt,
					     int depth, size_t *len);

/* Enforce (dynamic) linker error for version mismatches. Call from main. */
LUA_API void LUAJIT_VERSION_SYM(void);

#endif
