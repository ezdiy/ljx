/*
** String handling.
** Copyright (C) 2005-2020 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_STR_H
#define _LJ_STR_H

#include <stdarg.h>

#include "lj_obj.h"

/* String helpers. */
LJ_FUNC int32_t LJ_FASTCALL lj_str_cmp(GCstr *a, GCstr *b);
LJ_FUNC uint32_t lj_str_find(const char *s, const char *f,
				MSize slen, MSize flen, int32_t start);
LJ_FUNC int lj_str_haspattern(GCstr *s);

/* String interning. */
LJ_FUNC void lj_str_resize(lua_State *L, MSize newmask);
LJ_FUNCA GCstr *lj_str_new(lua_State *L, const char *str, size_t len);
LJ_FUNC void LJ_FASTCALL lj_str_free(global_State *g, GCstr *s);
LJ_FUNC void LJ_FASTCALL lj_str_init(lua_State *L);
#define lj_str_freetab(g) \
  (lj_mem_freevec(g, g->str.tab, g->str.mask+1, GCRef))

/* Actually lives in lib_string.c. */
MatchState * ljx_str_match(lua_State *L, const char *s, const char *p, MSize slen, MSize plen, int32_t start);

#define lj_str_newz(L, s)	(lj_str_new(L, s, strlen(s)))
#define lj_str_newlit(L, s)	(lj_str_new(L, "" s, sizeof(s)-1))
#define lj_str_size(len)	(sizeof(GCstr) + (((len)+4) & ~(MSize)3))

#endif
