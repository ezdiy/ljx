/*
** Public Lua/C API.
** Copyright (C) 2005-2020 Mike Pall. See Copyright Notice in luajit.h
**
** Adaption to Lua 5.2
** Copyright (C) 2014 Karel Tuma. See Copyright Notice in luajit.h
**
** Rest of port 5.3 and 5.4 port by ez in 2020.
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_api_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_udata.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_bc.h"
#include "lj_frame.h"
#include "lj_trace.h"
#include "lj_vm.h"
#include "lj_strscan.h"
#include "lj_strfmt.h"
#include "lj_char.h"


/* -- Common helper functions --------------------------------------------- */

#define lj_checkapi_slot(idx) \
  lj_checkapi((idx) <= (L->top - L->base), "stack slot %d out of range", (idx))

static TValue *index2adr(lua_State *L, int idx)
{
  TValue *o = &G(L)->tmptv;
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    return o < L->top ? o : niltv(L); 
  } else if (idx > LUA51_REGISTRYINDEX) {
    lj_checkapi(idx != 0 && -idx <= L->top - L->base,
        "bad stack slot %d", idx);
    return L->top + idx;
  } else if (idx == LUA_GLOBALSINDEX) {
    return (TValue*)lj_curr_env(L, 0);
  } else if (idx == LUA_REGISTRYINDEX || idx == LUA51_REGISTRYINDEX) {
    return registry(L);
  } else { 
    GCfunc *fn = curr_func(L);
    lj_checkapi(fn->c.gct == ~LJ_TFUNC && !isluafunc(fn),
        "calling frame is not a C function");
    if (idx == LUA_ENVIRONINDEX) {
      if (gcrefu(fn->c.env))
        return (TValue*)lj_tab_getint(tabV(registry(L)), LUA_RIDX_GLOBALS);
      settabref(L, o, fn->c.env);
      return o;
    } else {
      if (idx < LUA_UVINDEX)
        idx = LUA_UVINDEX - idx; 
      else if (idx < LUA51_UVINDEX)
        idx = LUA51_UVINDEX - idx;
      return (idx > 0 && idx <= fn->c.nupvalues) ? &fn->c.upvalue[idx-1] : niltv(L);
    }
  }
}


static LJ_AINLINE TValue *index2adr_check(lua_State *L, int idx)
{
  TValue *o = index2adr(L, idx);
  lj_checkapi(o != niltv(L), "invalid stack slot %d", idx);
  return o;
}

static TValue *index2adr_stack(lua_State *L, int idx)
{
  if (idx > 0) {
    TValue *o = L->base + (idx - 1);
    if (o < L->top) {
      return o;
    } else {
      lj_checkapi(0, "invalid stack slot %d", idx);
      return niltv(L);
    }
    return o < L->top ? o : niltv(L);
  } else {
    lj_checkapi(idx != 0 && -idx <= L->top - L->base,
		"invalid stack slot %d", idx);
    return L->top + idx;
  }
}

static GCtab *getcurrenv(lua_State *L)
{
  GCfunc *fn = curr_func(L);
  return fn->c.gct == ~LJ_TFUNC ? tabref(fn->c.env) : tabref(L->env);
}

/* -- Miscellaneous API functions ----------------------------------------- */

LUA_API int lua_status(lua_State *L)
{
  return L->status;
}

LUA_API int lua_checkstack(lua_State *L, int size)
{
  if (size > LUAI_MAXCSTACK || (L->top - L->base + size) > LUAI_MAXCSTACK) {
    return 0;  /* Stack overflow. */
  } else if (size > 0) {
    lj_state_checkstack(L, (MSize)size);
  }
  return 1;
}

LUALIB_API void luaL_checkstack(lua_State *L, int size, const char *msg)
{
  if (!lua_checkstack(L, size))
    lj_err_callerv(L, LJ_ERR_STKOVM, msg);
}

LUA_API void lua_xmove(lua_State *L, lua_State *to, int n)
{
  TValue *f, *t;
  if (L == to) return;
  lj_checkapi_slot(n);
  lj_checkapi(G(L) == G(to), "move across global states");
  lj_state_checkstack(to, (MSize)n);
  f = L->top;
  t = to->top = to->top + n;
  while (--n >= 0) copyTV(to, --t, --f);
  L->top = f;
}

/* -- Stack manipulation -------------------------------------------------- */

LUA_API int lua_absindex(lua_State *L, int idx)
{
  if ((idx > 0) || (idx <= LUAI_FIRSTPSEUDOIDX) || (idx <= LUA51_UVINDEX))
    return idx;
  return (int)(L->top - L->base + idx + 1);
}

static void reverse(lua_State *L, TValue *from, TValue *to)
{
  for (; from < to; from++, to--) {
    TValue temp;
    copyTV(L, &temp, from);
    copyTV(L, from, to);
    copyTV(L, to, &temp);
  }
}

LUA_API void lua_rotate (lua_State *L, int idx, int n) {
  TValue *p, *t, *m;
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2adr_stack(L, idx);  /* start of segment */
  lj_checkapi((n >= 0 ? n : -n) <= ((t - p) + 1), "invalid args lua_rotate(%d,%d)", idx, n);
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
}

LUA_API int lua_gettop(lua_State *L)
{
  return (int)(L->top - L->base);
}

LUA_API void lua_settop(lua_State *L, int idx)
{
  if (idx >= 0) {
    lj_checkapi(idx <= tvref(L->maxstack) - L->base, "bad stack slot %d", idx);
    if (L->base + idx > L->top) {
      if (L->base + idx >= tvref(L->maxstack))
      lj_state_growstack(L, (MSize)idx - (MSize)(L->top - L->base));
      do { setnilV(L->top++); } while (L->top < L->base + idx);
    } else {
      L->top = L->base + idx;
    }
  } else {
    lj_checkapi(-(idx+1) <= (L->top - L->base), "bad stack slot %d", idx);
    L->top += idx+1;  /* Shrinks top (idx < 0). */
  }
}

LUA_API void lua_remove(lua_State *L, int idx)
{
  TValue *p = index2adr_stack(L, idx);
  while (++p < L->top) copyTV(L, p-1, p);
  L->top--;
}

LUA_API void lua_insert(lua_State *L, int idx)
{
  TValue *q, *p = index2adr_stack(L, idx);
  for (q = L->top; q > p; q--) copyTV(L, q, q-1);
  copyTV(L, p, L->top);
}

static void copy_slot(lua_State *L, TValue *f, int idx)
{
  if (idx == LUA_GLOBALSINDEX) {
    lj_checkapi(tvistab(f), "stack slot %d is not a table", idx);
    /* NOBARRIER: A thread (i.e. L) is never black. */
    setgcref(L->env, obj2gco(tabV(f)));
  } else if (idx == LUA_ENVIRONINDEX) {
    GCfunc *fn = curr_func(L);
    if (fn->c.gct != ~LJ_TFUNC)
      lj_err_msg(L, LJ_ERR_NOENV);
    lj_checkapi(tvistab(f), "stack slot %d is not a table", idx);
    setgcref(fn->c.env, obj2gco(tabV(f)));
    lj_gc_barrier(L, fn, f);
  } else
  {
    TValue *o = index2adr_check(L, idx);
    copyTV(L, o, f);
    if (idx < LUA_UVINDEX || idx < LUA51_UVINDEX)  /* Need a barrier for upvalues. */
      lj_gc_barrier(L, curr_func(L), f);
  }
}

LUA_API void lua_replace(lua_State *L, int idx)
{
  lj_checkapi_slot(1);
  copy_slot(L, L->top - 1, idx);
  L->top--;
}

LUA_API void lua_copy(lua_State *L, int fromidx, int toidx)
{
  copy_slot(L, index2adr(L, fromidx), toidx);
}

LUA_API void lua_pushvalue(lua_State *L, int idx)
{
  copyTV(L, L->top, index2adr(L, idx));
  incr_top(L);
}

LUA_API size_t lua_stringtonumber (lua_State *L, const char *s)
{
  char* endptr;
  lua_Number n = lua_str2number(s, &endptr);
  if (endptr != s) {
    while (*endptr != '\0' && lj_char_isspace((unsigned char)*endptr))
      ++endptr;
    if (*endptr == '\0') {
      lua_pushnumber(L, n);
      return endptr - s + 1;
    }
  }
  return 0;
}

/* -- Stack getters ------------------------------------------------------- */

static int ljx_tv2type(lua_State *L, cTValue *o)
{
  if (!o) return LUA_TNIL;
  if (tvisnumber(o)) {
    return LUA_TNUMBER;
#if LJ_64 && !LJ_GC64
  } else if (tvislightud(o)) {
    return LUA_TLIGHTUSERDATA;
#endif
  } else if (o == niltv(L)) {
    return LUA_TNONE;
  } else {  /* Magic internal/external tag conversion. ORDER LJ_T */
    uint32_t t = ~itype(o);
#if LJ_64
    int tt = (int)((U64x(75a06,98042110) >> 4*t) & 15u);
#else
    int tt = (int)(((t < 8 ? 0x98042110u : 0x75a06u) >> 4*(t&7)) & 15u);
#endif
    lj_assertL(tt != LUA_TNIL || tvisnil(o), "bad tag conversion");
    return tt;
  }
}

LUA_API int lua_type(lua_State *L, int idx)
{
  return ljx_tv2type(L, index2adr(L, idx));
}

LUALIB_API void luaL_checktype(lua_State *L, int idx, int tt)
{
  if (lua_type(L, idx) != tt)
    lj_err_argt(L, idx, tt);
}

LUALIB_API void luaL_checkany(lua_State *L, int idx)
{
  if (index2adr(L, idx) == niltv(L))
    lj_err_arg(L, idx, LJ_ERR_NOVAL);
}

LUA_API const char *lua_typename(lua_State *L, int t)
{
  UNUSED(L);
  return lj_obj_typename[t+1];
}

LUA_API int lua_iscfunction(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return tvisfunc(o) && !isluafunc(funcV(o));
}

LUA_API int lua_isnumber(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  return (tvisnumber(o) || (tvisstr(o) && lj_strscan_number(strV(o), &tmp)));
}

LUA_API int lua_isinteger(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return tvisint(o) || (tvisnum(o) && numV(o) == (lua_Integer) numV(o));
}

LUA_API int lua_isstring(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return (tvisstr(o) || tvisnumber(o));
}

LUA_API int lua_isuserdata(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return (tvisudata(o) || tvislightud(o));
}

LUA_API int lua_rawequal(lua_State *L, int idx1, int idx2)
{
  cTValue *o1 = index2adr(L, idx1);
  cTValue *o2 = index2adr(L, idx2);
  return (o1 == niltv(L) || o2 == niltv(L)) ? 0 : lj_obj_equal(o1, o2);
}

LUA_API int lua_equal(lua_State *L, int idx1, int idx2)
{
  cTValue *o1 = index2adr(L, idx1);
  cTValue *o2 = index2adr(L, idx2);
  if (tvisint(o1) && tvisint(o2)) {
    return intV(o1) == intV(o2);
  } else if (tvisnumber(o1) && tvisnumber(o2)) {
    return numberVnum(o1) == numberVnum(o2);
  } else if (itype(o1) != itype(o2)) {
    return 0;
  } else if (tvispri(o1)) {
    return o1 != niltv(L) && o2 != niltv(L);
#if LJ_64 && !LJ_GC64
  } else if (tvislightud(o1)) {
    return o1->u64 == o2->u64;
#endif
  } else if (gcrefeq(o1->gcr, o2->gcr)) {
    return 1;
  } else if (!tvistabud(o1)) {
    return 0;
  } else {
    TValue *base = lj_meta_equal(L, gcV(o1), gcV(o2), 0);
    if ((uintptr_t)base <= 1) {
      return (int)(uintptr_t)base;
    } else {
      L->top = base+2;
      lj_vm_call(L, base, 1+1);
      L->top -= 2+LJ_FR2;
      return tvistruecond(L->top+1+LJ_FR2);
    }
  }
}

LUA_API int lua_lessthan(lua_State *L, int idx1, int idx2)
{
  cTValue *o1 = index2adr(L, idx1);
  cTValue *o2 = index2adr(L, idx2);
  if (o1 == niltv(L) || o2 == niltv(L)) {
    return 0;
  } else if (tvisint(o1) && tvisint(o2)) {
    return intV(o1) < intV(o2);
  } else if (tvisnumber(o1) && tvisnumber(o2)) {
    return numberVnum(o1) < numberVnum(o2);
  } else {
    TValue *base = lj_meta_comp(L, o1, o2, 0);
    if ((uintptr_t)base <= 1) {
      return (int)(uintptr_t)base;
    } else {
      L->top = base+2;
      lj_vm_call(L, base, 1+1);
      L->top -= 2+LJ_FR2;
      return tvistruecond(L->top+1+LJ_FR2);
    }
  }
}

LUA_API int lua_compare(lua_State *L, int index1, int index2, int op)
{
  switch (op) {
    case LUA_OPEQ: return lua_equal(L, index1, index2);
    case LUA_OPLT: return lua_lessthan(L, index1, index2);
    case LUA_OPLE: return lua_lessthan(L, index1, index2) || lua_equal(L, index1, index2);
  }
  lj_checkapi(0, "op = LUA_OP* expected");
  return 0; /* Should not be reached. */
}

LUA_API lua_Number lua_tonumberx(lua_State *L, int idx, int *ok)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(tvisnumber(o))) {
    if (ok) *ok = 1;
    return numberVnum(o);
  } else if (tvisstr(o) && lj_strscan_num(strV(o), &tmp)) {
    if (ok) *ok = 1;
    return numV(&tmp);
  } else {
    if (ok) *ok = 0;
    return 0;
  }
}

LUA_API lua_Number lua_tonumber(lua_State *L, int idx)
{
  return lua_tonumberx(L, idx, NULL);
}

LUALIB_API lua_Number luaL_checknumber(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(tvisnumber(o)))
    return numberVnum(o);
  else if (!(tvisstr(o) && lj_strscan_num(strV(o), &tmp)))
    lj_err_argt(L, idx, LUA_TNUMBER);
  return numV(&tmp);
}

LUALIB_API lua_Number luaL_optnumber(lua_State *L, int idx, lua_Number def)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  if (LJ_LIKELY(tvisnumber(o)))
    return numberVnum(o);
  else if (tvisnil(o))
    return def;
  else if (!(tvisstr(o) && lj_strscan_num(strV(o), &tmp)))
    lj_err_argt(L, idx, LUA_TNUMBER);
  return numV(&tmp);
}

LUA_API LUA_INTEGER64 lua_tointeger(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  lua_Number n;
  if (LJ_LIKELY(tvisint(o))) {
    return intV(o);
  } else if (LJ_LIKELY(tvisnum(o))) {
    n = numV(o);
  } else {
    if (!(tvisstr(o) && lj_strscan_number(strV(o), &tmp)))
      return 0;
    if (tvisint(&tmp))
      return intV(&tmp);
    n = numV(&tmp);
  }
  return (LUA_INTEGER64)n;
}

LUA_API LUA_INTEGER64 lua_tointegerx(lua_State *L, int idx, int *ok)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  lua_Number n;
  if (LJ_LIKELY(tvisint(o))) {
    if (ok) *ok = 1;
    return intV(o);
  } else if (LJ_LIKELY(tvisnum(o))) {
    n = numV(o);
  } else {
    if (!(tvisstr(o) && lj_strscan_number(strV(o), &tmp))) {
      if (ok) *ok = 0;
      return 0;
    }
    if (tvisint(&tmp)) {
      if (ok) *ok = 1;
      return intV(&tmp);
    }
    n = numV(&tmp);
  }
  if (ok) *ok = 1;
  return (LUA_INTEGER64)n;
}

LUALIB_API LUA_INTEGER64 luaL_checkinteger(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  lua_Number n;
  if (LJ_LIKELY(tvisint(o))) {
    return intV(o);
  } else if (LJ_LIKELY(tvisnum(o))) {
    n = numV(o);
  } else {
    if (!(tvisstr(o) && lj_strscan_number(strV(o), &tmp)))
      lj_err_argt(L, idx, LUA_TNUMBER);
    if (tvisint(&tmp))
      return (lua_Integer)intV(&tmp);
    n = numV(&tmp);
  }
  return (LUA_INTEGER64)n;
}

LUALIB_API LUA_INTEGER64 luaL_optinteger(lua_State *L, int idx, lua_Integer def)
{
  cTValue *o = index2adr(L, idx);
  TValue tmp;
  lua_Number n;
  if (LJ_LIKELY(tvisint(o))) {
    return intV(o);
  } else if (LJ_LIKELY(tvisnum(o))) {
    n = numV(o);
  } else if (tvisnil(o)) {
    return def;
  } else {
    if (!(tvisstr(o) && lj_strscan_number(strV(o), &tmp)))
      lj_err_argt(L, idx, LUA_TNUMBER);
    if (tvisint(&tmp))
      return (lua_Integer)intV(&tmp);
    n = numV(&tmp);
  }
  return (LUA_INTEGER64)n;
}

LUA_API LUA_UNSIGNED64 lua_tounsigned(lua_State *L, int idx)
{
  return (LUA_UNSIGNED64) lua_tointeger(L, idx);
}
LUA_API LUA_UNSIGNED64 lua_tounsignedx(lua_State *L, int idx, int *succ)
{
  return (LUA_UNSIGNED64) lua_tointegerx(L, idx, succ);
}
LUALIB_API LUA_UNSIGNED64 luaL_optunsigned(lua_State *L, int idx, lua_Unsigned def)
{
  return (LUA_UNSIGNED64) luaL_optinteger(L, idx, def);
}
LUALIB_API LUA_UNSIGNED64 luaL_checkunsigned (lua_State *L, int narg) {
  int isnum;
  LUA_UNSIGNED64 d = lua_tounsignedx(L, narg, &isnum);
  if (!isnum)
    lj_err_argt(L, narg, LUA_TNUMBER);
  return d;
}

LUA_API int lua_toboolean(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return tvistruecond(o);
}

LUA_API const char *lua_tolstring(lua_State *L, int idx, size_t *len)
{
  TValue *o = index2adr(L, idx);
  GCstr *s;
  if (LJ_LIKELY(tvisstr(o))) {
    s = strV(o);
  } else if (tvisnumber(o)) {
    lj_gc_check(L);
    o = index2adr(L, idx);  /* GC may move the stack. */
    s = lj_strfmt_number(L, o);
    setstrV(L, o, s);
  } else {
    if (len != NULL) *len = 0;
    return NULL;
  }
  if (len != NULL) *len = s->len;
  return strdata(s);
}

LUALIB_API const char *luaL_checklstring(lua_State *L, int idx, size_t *len)
{
  const char *res = lua_tolstring(L, idx, len);
  if (!res) lj_err_argt(L, idx, LUA_TSTRING);
  return res;
}


LUALIB_API const char *luaL_optlstring(lua_State *L, int idx,
				       const char *def, size_t *len)
{
  TValue *o = index2adr(L, idx);
  GCstr *s;
  if (LJ_LIKELY(tvisstr(o))) {
    s = strV(o);
  } else if (tvisnil(o)) {
    if (len != NULL) *len = def ? strlen(def) : 0;
    return def;
  } else if (tvisnumber(o)) {
    lj_gc_check(L);
    o = index2adr(L, idx);  /* GC may move the stack. */
    s = lj_strfmt_number(L, o);
    setstrV(L, o, s);
  } else {
    lj_err_argt(L, idx, LUA_TSTRING);
  }
  if (len != NULL) *len = s->len;
  return strdata(s);
}

LUALIB_API int luaL_checkoption(lua_State *L, int idx, const char *def,
				const char *const lst[])
{
  ptrdiff_t i;
  const char *s = lua_tolstring(L, idx, NULL);
  if (s == NULL && (s = def) == NULL)
    lj_err_argt(L, idx, LUA_TSTRING);
  for (i = 0; lst[i]; i++)
    if (strcmp(lst[i], s) == 0)
      return (int)i;
  lj_err_argv(L, idx, LJ_ERR_INVOPTM, s);
}

LUA_API size_t lua_rawlen(lua_State *L, int idx)
{
  TValue *o = index2adr(L, idx);
  if (tvisstr(o)) {
    return strV(o)->len;
  } else if (tvistab(o)) {
    return (size_t)lj_tab_len(tabV(o));
  } else if (tvisudata(o)) {
    return udataV(o)->len;
  }
  return 0;
}

LUA_API size_t lua_objlen(lua_State *L, int idx)
{
  TValue *o = index2adr(L, idx);
  if (tvisnumber(o)) {
    GCstr *s = lj_strfmt_number(L, o);
    setstrV(L, o, s);
    return s->len;
  }
  return lua_rawlen(L, idx);
}

LUA_API lua_CFunction lua_tocfunction(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  if (tvisfunc(o)) {
    BCOp op = bc_op(*mref(funcV(o)->c.pc, BCIns));
    if (op == BC_FUNCC || op == BC_FUNCCW)
      return funcV(o)->c.f;
  }
  return NULL;
}

LUA_API void *lua_touserdata(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  if (tvisudata(o))
    return uddata(udataV(o));
  else if (tvislightud(o))
    return lightudV(o);
  else
    return NULL;
}

LUA_API lua_State *lua_tothread(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  return (!tvisthread(o)) ? NULL : threadV(o);
}

LUA_API const void *lua_topointer(lua_State *L, int idx)
{
  return lj_obj_ptr(index2adr(L, idx));
}

/* -- Stack setters (object creation) ------------------------------------- */

LUA_API void lua_pushnil(lua_State *L)
{
  setnilV(L->top);
  incr_top(L);
}

LUA_API void lua_pushnumber(lua_State *L, lua_Number n)
{
  setnumV(L->top, n);
  if (LJ_UNLIKELY(tvisnan(L->top)))
    setnanV(L->top);  /* Canonicalize injected NaNs. */
  incr_top(L);
}

LUA_API void lua_pushinteger(lua_State *L, lua_Integer n)
{
  setintptrV(L->top, n);
  incr_top(L);
}

/* TBD: casts */
LUA_API void lua_pushunsigned(lua_State *L, lua_Unsigned n)
{
  setint64V(L->top, n);
  incr_top(L);
}


LUA_API void lua_pushlstring(lua_State *L, const char *str, size_t len)
{
  GCstr *s;
  lj_gc_check(L);
  s = lj_str_new(L, str, len);
  setstrV(L, L->top, s);
  incr_top(L);
}

LUA_API void lua_pushstring(lua_State *L, const char *str)
{
  if (str == NULL) {
    setnilV(L->top);
  } else {
    GCstr *s;
    lj_gc_check(L);
    s = lj_str_newz(L, str);
    setstrV(L, L->top, s);
  }
  incr_top(L);
}

LUA_API const char *lua_pushvfstring(lua_State *L, const char *fmt,
				     va_list argp)
{
  lj_gc_check(L);
  return lj_strfmt_pushvf(L, fmt, argp);
}

LUA_API const char *lua_pushfstring(lua_State *L, const char *fmt, ...)
{
  const char *ret;
  va_list argp;
  lj_gc_check(L);
  va_start(argp, fmt);
  ret = lj_strfmt_pushvf(L, fmt, argp);
  va_end(argp);
  return ret;
}

LUA_API void lua_pushcclosure(lua_State *L, lua_CFunction f, int n)
{
  GCfunc *fn;
  lj_gc_check(L);
  lj_checkapi_slot(n);
  fn = lj_func_newC(L, (MSize)n, getcurrenv(L));
  fn->c.f = f;
  L->top -= n;
  while (n--)
    copyTV(L, &fn->c.upvalue[n], L->top+n);
  setfuncV(L, L->top, fn);
  lj_assertL(iswhite(obj2gco(fn)), "new GC object is not white");
  incr_top(L);
}

LUA_API void lua_pushboolean(lua_State *L, int b)
{
  setboolV(L->top, (b != 0));
  incr_top(L);
}

LUA_API void lua_pushlightuserdata(lua_State *L, void *p)
{
  setlightudV(L->top, checklightudptr(L, p));
  incr_top(L);
}

LUA_API void lua_pushuserdata_native(lua_State *L, void *p)
{
  setudataV(L, L->top, checklightudptr(L, p)); /* Checks 1<<47 range, not type. */
  incr_top(L);
}

LUA_API void lua_createtable(lua_State *L, int narray, int nrec)
{
  lj_gc_check(L);
  settabV(L, L->top, lj_tab_new_ah(L, narray, nrec));
  incr_top(L);
}

LUALIB_API int luaL_newmetatable(lua_State *L, const char *tname)
{
  GCtab *regt = tabV(registry(L));
  TValue *tv = lj_tab_setstr(L, regt, lj_str_newz(L, tname));
  if (tvisnil(tv)) {
    GCtab *mt = lj_tab_new(L, 0, 1);
    settabV(L, tv, mt);
    settabV(L, L->top++, mt);
    lj_gc_anybarriert(L, regt);
    return 1;
  } else {
    copyTV(L, L->top++, tv);
    return 0;
  }
}

LUA_API int lua_pushthread(lua_State *L)
{
  setthreadV(L, L->top, L);
  incr_top(L);
  return (mainthread(G(L)) == L);
}

LUA_API lua_State *lua_newthread(lua_State *L)
{
  lua_State *L1;
  lj_gc_check(L);
  L1 = lj_state_new(L);
  setthreadV(L, L->top, L1);
  incr_top(L);
  return L1;
}

LUA_API void *lua_newuserdatauv(lua_State *L, size_t size, int nval)
{
  GCudata *ud;
  lj_gc_check(L);
  if (size > LJ_MAX_UDATA)
    lj_err_msg(L, LJ_ERR_UDATAOV);
  ud = lj_udata_new(L, (MSize)size, getcurrenv(L));
  setudataV(L, L->top, ud);
  incr_top(L);
  return uddata(ud);
}

LUA_API void *lua_newuserdata(lua_State *L, size_t size)
{
  return lua_newuserdatauv(L, size, 0);
}

LUA_API void lua_concat(lua_State *L, int n)
{
  lj_checkapi_slot(n);
  if (n >= 2) {
    n--;
    do {
      TValue *top = lj_meta_cat(L, L->top-1, -n);
      if (top == NULL) {
	L->top -= n;
	break;
      }
      n -= (int)(L->top - top);
      L->top = top+2;
      lj_vm_call(L, top, 1+1);
      L->top -= 1+LJ_FR2;
      copyTV(L, L->top-1, L->top+LJ_FR2);
    } while (--n > 0);
  } else if (n == 0) {  /* Push empty string. */
    setstrV(L, L->top, &G(L)->strempty);
    incr_top(L);
  }
  /* else n == 1: nothing to do. */
}

/* -- Object getters ------------------------------------------------------ */

LUA_API int lua_gettable(lua_State *L, int idx)
{
  cTValue *t = index2adr_check(L, idx);
  cTValue *v = lj_meta_tget(L, t, L->top-1);
  if (v == NULL) {
    L->top += 2;
    lj_vm_call(L, L->top-2, 1+1);
    L->top -= 2+LJ_FR2;
    v = L->top+1+LJ_FR2;
  }
  copyTV(L, L->top-1, v);
  return ljx_tv2type(L, v);
}

LUA_API int lua_geti(lua_State *L, int idx, lua_Integer i)
{
  cTValue *v, *t = index2adr(L, idx);
  if (!(v = lj_tab_getint(tabV(t), i))) {
    TValue k;
    setnumV(&k, (lua_Number)i);
    v = lj_meta_tget(L, t, &k);
  }
  if (v == NULL) {
    L->top += 2;
    lj_vm_call(L, L->top-2, 1+1);
    L->top -= 2+LJ_FR2;
    v = L->top+1+LJ_FR2;
  }
  copyTV(L, L->top-1, v);
  return ljx_tv2type(L, v);
}

LUA_API int lua_getfield(lua_State *L, int idx, const char *k)
{
  cTValue *v, *t = index2adr_check(L, idx);
  TValue key;
  setstrV(L, &key, lj_str_newz(L, k));
  v = lj_meta_tget(L, t, &key);
  if (v == NULL) {
    L->top += 2;
    lj_vm_call(L, L->top-2, 1+1);
    L->top -= 2+LJ_FR2;
    v = L->top+1+LJ_FR2;
  }
  copyTV(L, L->top, v);
  incr_top(L);
  return ljx_tv2type(L, v);
}

LUA_API int lua_rawget(lua_State *L, int idx)
{
  cTValue *v, *t = index2adr(L, idx);
  lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
  copyTV(L, L->top-1, (v = lj_tab_get(L, tabV(t), L->top-1)));
  return ljx_tv2type(L, v);
}


LUA_API int lua_rawgeti(lua_State *L, int idx, int n)
{
  cTValue *v, *t = index2adr(L, idx);
  lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
  v = lj_tab_getint(tabV(t), n);
  if (v) {
    copyTV(L, L->top, v);
  } else {
    setnilV(L->top);
  }
  incr_top(L);
  return ljx_tv2type(L, v);
}

LUA_API int lua_rawgetp(lua_State *L, int idx, const void *p)
{
  cTValue *v, *t = index2adr(L, idx);
  TValue key;
  lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
  setlightudV(&key, (void*)p);
  v = lj_tab_get(L, tabV(t), &key);
  if (v) {
    copyTV(L, L->top, v);
  } else {
    setnilV(L->top);
  }
  incr_top(L);
  return ljx_tv2type(L, v);
}


LUA_API int lua_getmetatable(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  GCtab *mt = NULL;
  if (tvistab(o))
    mt = tabref(tabV(o)->metatable);
  else if (tvisudata(o))
    mt = tabref(udataV(o)->metatable);
  else
    mt = tabref(basemt_obj(G(L), o));
  if (mt == NULL)
    return 0;
  settabV(L, L->top, mt);
  incr_top(L);
  return 1;
}

LUALIB_API int luaL_getmetafield(lua_State *L, int idx, const char *field)
{
  if (lua_getmetatable(L, idx)) {
    cTValue *tv = lj_tab_getstr(tabV(L->top-1), lj_str_newz(L, field));
    if (tv && !tvisnil(tv)) {
      copyTV(L, L->top-1, tv);
      return ljx_tv2type(L, tv);
    }
    L->top--;
  }
  return LUA_TNIL;
}

LUA_API void lua_getfenv(lua_State *L, int idx)
{
  cTValue *o = index2adr_check(L, idx);
  if (tvisfunc(o)) {
    settabref(L, L->top, funcV(o)->c.env);
  } else if (tvisudata(o)) {
    getuservalue(L, udataV(o), L->top);
  } else if (tvisthread(o)) {
    settabref(L, L->top, threadV(o)->env);
  } else {
    setnilV(L->top);
  }
  incr_top(L);
}

LUA_API int lua_getuservalue(lua_State *L, int idx)
{
  cTValue *s, *o = index2adr_check(L, idx);
  s = lj_tab_get(L, tabV(lj_tab_getint(tabV(registry(L)), LUA_RIDX_USERVAL)), o);
  if (!s)
    setnilV(L->top);
  else
    copyTV(L, L->top, s);
  incr_top(L);
  return ljx_tv2type(L, L->top-1);
}

LUA_API int lua_getiuservalue(lua_State *L, int idx, int n)
{
  n -= 2;
  if (n < 0)
    return lua_getuservalue(L, idx);
  cTValue *s, *o = index2adr_check(L, idx);
  s = lj_tab_get(L, tabV(lj_tab_getint(tabV(registry(L)), LUA_RIDX_IUSERVAL)), o);
  setnilV(L->top);
  if (tvistab(s)) {
    GCtab *tt = tabV(s);
    if (inarray(tt, n))
      copyTV(L, L->top, arrayslot(tt, n));
  }
  incr_top(L);
  return ljx_tv2type(L, L->top-1);
}

LUA_API int lua_next(lua_State *L, int idx)
{
  cTValue *t = index2adr(L, idx);
  int more;
  lj_checkapi(tvistab(t), "stack slot %d is not a table", idx);
  more = lj_tab_next(L, tabV(t), L->top-1);
  if (more) {
    incr_top(L);  /* Return new key and value slot. */
  } else {  /* End of traversal. */
    L->top--;  /* Remove key slot. */
  }
  return more;
}

LUA_API const char *lua_getupvalue(lua_State *L, int idx, int n)
{
  TValue *val;
  GCobj *o;
  const char *name = lj_debug_uvnamev(index2adr(L, idx), (uint32_t)(n-1), &val, &o);
  if (name) {
    copyTV(L, L->top, val);
    incr_top(L);
  }
  return name;
}

LUA_API void *lua_upvalueid(lua_State *L, int idx, int n)
{
  GCfunc *fn = funcV(index2adr(L, idx));
  n--;
  lj_checkapi((uint32_t)n < fn->l.nupvalues, "bad upvalue %d", n);
  return isluafunc(fn) ? (void *)gcref(fn->l.uvptr[n]) :
			 (void *)&fn->c.upvalue[n];
}

LUA_API void lua_upvaluejoin(lua_State *L, int idx1, int n1, int idx2, int n2)
{
  GCfunc *fn1 = funcV(index2adr(L, idx1));
  GCfunc *fn2 = funcV(index2adr(L, idx2));
  n1--; n2--;
  lj_checkapi(isluafunc(fn1), "stack slot %d is not a Lua function", idx1);
  lj_checkapi(isluafunc(fn2), "stack slot %d is not a Lua function", idx2);
  lj_checkapi((uint32_t)n1 < fn1->l.nupvalues, "bad upvalue %d", n1+1);
  lj_checkapi((uint32_t)n2 < fn2->l.nupvalues, "bad upvalue %d", n2+1);
  setgcrefr(fn1->l.uvptr[n1], fn2->l.uvptr[n2]);
  lj_gc_objbarrier(L, fn1, gcref(fn1->l.uvptr[n1]));
}

LUALIB_API void *luaL_testudata(lua_State *L, int idx, const char *tname)
{
  cTValue *o = index2adr(L, idx);
  if (tvisudata(o)) {
    GCudata *ud = udataV(o);
    cTValue *tv = lj_tab_getstr(tabV(registry(L)), lj_str_newz(L, tname));
    if (tv && tvistab(tv) && tabV(tv) == tabref(ud->metatable))
      return uddata(ud);
  }
  return NULL;  /* value is not a userdata with a metatable */
}

LUALIB_API void *luaL_checkudata(lua_State *L, int idx, const char *tname)
{
  void *p = luaL_testudata(L, idx, tname);
  if (!p) lj_err_argtype(L, idx, tname);
  return p;
}

/* -- Object setters ------------------------------------------------------ */
LUA_API void lua_settable(lua_State *L, int idx)
{
  TValue *o;
  cTValue *t = index2adr_check(L, idx);
  lj_checkapi_slot(2);
  o = lj_meta_tset(L, t, L->top-2);
  if (o) {
    /* NOBARRIER: lj_meta_tset ensures the table is not black. */
    L->top -= 2;
    copyTV(L, o, L->top+1);
  } else {
    TValue *base = L->top;
    copyTV(L, base+2, base-3-2*LJ_FR2);
    L->top = base+3;
    lj_vm_call(L, base, 0+1);
    L->top -= 3+LJ_FR2;
  }
}

LUA_API void lua_setfield(lua_State *L, int idx, const char *k)
{
  TValue *o;
  TValue key;
  cTValue *t = index2adr_check(L, idx);
  lj_checkapi_slot(1);
  setstrV(L, &key, lj_str_newz(L, k));
  o = lj_meta_tset(L, t, &key);
  if (o) {
    /* NOBARRIER: lj_meta_tset ensures the table is not black. */
    copyTV(L, o, --L->top);
  } else {
    TValue *base = L->top;
    copyTV(L, base+2, base-3-2*LJ_FR2);
    L->top = base+3;
    lj_vm_call(L, base, 0+1);
    L->top -= 2+LJ_FR2;
  }
}

LUA_API void lua_seti(lua_State *L, int idx, lua_Integer i)
{
  TValue *o, k;
  cTValue *t = index2adr_check(L, idx);
  setnumV(&k, (lua_Number)i);
  o = lj_meta_tset(L, t, &k);
  if (o) {
    /* NOBARRIER: lj_meta_tset ensures the table is not black. */
    copyTV(L, o, --L->top);
  } else {
    TValue *base = L->top;
    copyTV(L, base+2, base-3-2*LJ_FR2);
    L->top = base+3;
    lj_vm_call(L, base, 0+1);
    L->top -= 2+LJ_FR2;
  }
}

LUA_API void lua_setglobal (lua_State *L, const char *var)
{
  lua_setfield(L, LUA_GLOBALSINDEX, var);
}

LUA_API int lua_getglobal (lua_State *L, const char *var)
{
  return lua_getfield(L, LUA_GLOBALSINDEX, var);
}

LUA_API void lua_rawset(lua_State *L, int idx)
{
  GCtab *t = tabV(index2adr(L, idx));
  TValue *dst, *key;
  lj_checkapi_slot(2);
  key = L->top-2;
  dst = lj_tab_set(L, t, key);
  copyTV(L, dst, key+1);
  lj_gc_anybarriert(L, t);
  L->top = key;
}

LUA_API void lua_rawsetp(lua_State *L, int idx, const void *p)
{
  GCtab *t = tabV(index2adr_check(L, idx));
  TValue *dst, key;
  setlightudV(&key, (void*)p);
  dst = lj_tab_set(L, t, &key);
  copyTV(L, dst, L->top-1);
  lj_gc_anybarriert(L, t);
  L->top--;
}

LUA_API void lua_rawseti(lua_State *L, int idx, int n)
{
  GCtab *t = tabV(index2adr(L, idx));
  TValue *dst, *src;
  lj_checkapi_slot(1);
  dst = lj_tab_setint(L, t, n);
  src = L->top-1;
  copyTV(L, dst, src);
  lj_gc_barriert(L, t, dst);
  L->top = src;
}

LUA_API int lua_setmetatable(lua_State *L, int idx)
{
  global_State *g;
  GCtab *mt;
  cTValue *o = index2adr_check(L, idx);
  lj_checkapi_slot(1);
  if (tvisnil(L->top-1)) {
    mt = NULL;
  } else {
    lj_checkapi(tvistab(L->top-1), "top stack slot is not a table");
    mt = tabV(L->top-1);
  }
  g = G(L);
  if (tvistab(o)) {
    setgcref(tabV(o)->metatable, obj2gco(mt));
    if (mt) {
      if (lj_meta_fast(L, mt, MM_gc))
        lj_gc_tab_finalized(L, gcval(o));
      lj_gc_objbarriert(L, tabV(o), mt);
    }
  } else if (tvisudata(o)) {
    setgcref(udataV(o)->metatable, obj2gco(mt));
    if (mt) {
      /* Only 5.3 has ressurections */
#if LJ_53
      if (lj_meta_fast(L, mt, MM_gc))
	clearfinalized(gcval(o)); /* Resurrect. */
#endif
      lj_gc_objbarrier(L, gcV(o), mt);
    }
  } else {
    /* Flush cache, since traces specialize to basemt. But not during __gc. */
    if (lj_trace_flushall(L))
      lj_err_caller(L, LJ_ERR_NOGCMM);
    if (tvisbool(o)) {
      /* NOBARRIER: basemt is a GC root. */
      setgcref(basemt_it(g, LJ_TTRUE), obj2gco(mt));
      setgcref(basemt_it(g, LJ_TFALSE), obj2gco(mt));
    } else {
      /* NOBARRIER: basemt is a GC root. */
      setgcref(basemt_obj(g, o), obj2gco(mt));
    }
  }
  L->top--;
  return 1;
}

LUALIB_API void luaL_setmetatable(lua_State *L, const char *tname)
{
  lua_getfield(L, LUA_REGISTRYINDEX, tname);
  lua_setmetatable(L, -2);
}

LUA_API int lua_setfenv(lua_State *L, int idx)
{
  cTValue *o = index2adr(L, idx);
  GCtab *t;
  lj_checkapi_slot(1);
  if (tvisudata(o)) {
    lj_checkapi(tvisgcv(L->top-1), "top stack slot must be a gc (non-primitive) object");
    setuservalue(L, udataV(o), L->top-1);
  } else {
    lj_checkapi(tvistab(L->top-1), "top stack slot is not a table");
    t = tabV(L->top-1);
    if (tvisfunc(o)) {
      setgcref(funcV(o)->c.env, obj2gco(t));
    } else if (tvisthread(o)) {
      setgcref(threadV(o)->env, obj2gco(t));
    } else {
      L->top--;
      return 0;
    }
  }
  lj_gc_objbarrier(L, gcV(o), gcV(L->top-1));
  L->top--;
  return 1;
}

/* Set user value for gco at idx.
 * Any gc-able object can have userval associated; not just userdata. */
LUA_API void lua_setuservalue(lua_State *L, int idx)
{
  cTValue *o = index2adr_check(L, idx);
  lj_checkapi(tvisgcv(o), "idx must be a gc (non-primitive) object");
  lj_checkapi_slot(1);
  GCtab *userval = tabV(lj_tab_getint(tabV(registry(L)), LUA_RIDX_USERVAL));
  TValue *d = lj_tab_set(L, userval, o);
  copyTV(L, d, L->top-1);
  lj_gc_anybarriert(L, userval);
  L->top--;
}

LUA_API int lua_setiuservalue(lua_State *L, int idx, int n)
{
  cTValue *o = index2adr_check(L, idx);
  n -= 2;
  if (n >= 0) {
    lj_checkapi_slot(1);
    lj_checkapi(tvisgcv(o), "idx must be a gc (non-primitive) object");
    GCtab *tt = tabV(lj_tab_getint(tabV(registry(L)), LUA_RIDX_IUSERVAL));
    TValue *d = lj_tab_set(L, tt, o);
    if (!tvistab(d)) {
      settabV(L, d, lj_tab_new(L, n+1, 0));
      lj_gc_anybarriert(L, tt);
    }
    GCtab *dt = tabV(d);
    if (!inarray(dt, n))
      lj_tab_reasize(L, dt, n);
    lj_assertL(inarray(dt, n), "iuserdata table inconsistent");
    copyTV(L, lj_tab_setint(L, dt, (int32_t)n), L->top-1);
    lj_gc_anybarriert(L, dt);
    L->top--;
  } else lua_setuservalue(L, idx);
  return 1;
}

LUA_API const char *lua_setupvalue(lua_State *L, int idx, int n)
{
  cTValue *f = index2adr(L, idx);
  TValue *val;
  GCobj *o;
  const char *name;
  lj_checkapi_slot(1);
  name = lj_debug_uvnamev(f, (uint32_t)(n-1), &val, &o);
  if (name) {
    L->top--;
    copyTV(L, val, L->top);
    lj_gc_barrier(L, o, L->top);
  }
  return name;
}



/* -- Calls --------------------------------------------------------------- */

#if LJ_FR2
static TValue *api_call_base(lua_State *L, int nargs)
{
  TValue *o = L->top, *base = o - nargs;
  L->top = o+1;
  for (; o > base; o--) copyTV(L, o, o-1);
  setnilV(o);
  return o+1;
}
#else
#define api_call_base(L, nargs)	(L->top - (nargs))
#endif

LUA_API void lua_call(lua_State *L, int nargs, int nresults)
{
  lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
	      "thread called in wrong state %d", L->status);
  lj_checkapi_slot(nargs+1);
  lj_vm_call(L, api_call_base(L, nargs), nresults+1);
}

LUA_API int lua_pcall(lua_State *L, int nargs, int nresults, int errfunc)
{
  global_State *g = G(L);
  uint8_t oldh = hook_save(g);
  ptrdiff_t ef;
  int status;
  lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
	      "thread called in wrong state %d", L->status);
  lj_checkapi_slot(nargs+1);
  if (errfunc == 0) {
    ef = 0;
  } else {
    cTValue *o = index2adr_stack(L, errfunc);
    ef = savestack(L, o);
  }
  status = lj_vm_pcall(L, api_call_base(L, nargs), nresults+1, ef);
  if (status) hook_restore(g, oldh);
  return status;
}

static TValue *cpcall(lua_State *L, lua_CFunction func, void *ud)
{
  GCfunc *fn = lj_func_newC(L, 0, getcurrenv(L));
  TValue *top = L->top;
  fn->c.f = func;
  setfuncV(L, top++, fn);
  if (LJ_FR2) setnilV(top++);
  setlightudV(top++, checklightudptr(L, ud));
  cframe_nres(L->cframe) = 1+0;  /* Zero results. */
  L->top = top;
  return top-1;  /* Now call the newly allocated C function. */
}

LUA_API int lua_cpcall(lua_State *L, lua_CFunction func, void *ud)
{
  global_State *g = G(L);
  uint8_t oldh = hook_save(g);
  int status;
  lj_checkapi(L->status == LUA_OK || L->status == LUA_ERRERR,
	      "thread called in wrong state %d", L->status);
  status = lj_vm_cpcall(L, func, ud, cpcall);
  if (status) hook_restore(g, oldh);
  return status;
}

LUALIB_API int luaL_callmeta(lua_State *L, int idx, const char *field)
{
  if (luaL_getmetafield(L, idx, field)) {
    TValue *top = L->top--;
    if (LJ_FR2) setnilV(top++);
    copyTV(L, top++, index2adr(L, idx));
    L->top = top;
    lj_vm_call(L, top-1, 1+1);
    return 1;
  }
  return 0;
}

LUALIB_API void lua_len(lua_State *L, int i) {
  switch (lua_type(L, i)) {
    case LUA_TSTRING: /* fall through */
    case LUA_TTABLE:
      if (luaL_callmeta(L, i, "__len"))
        break;
      lua_pushunsigned(L, lua_objlen(L, i));
      break;
    case LUA_TUSERDATA:
      if (luaL_callmeta(L, i, "__len"))
        break;
      /* maybe fall through */
    default:
      lj_err_callerv(L, LJ_ERR_BADLEN, lua_typename(L, lua_type(L, i)));
  }
}

LUALIB_API int luaL_len(lua_State *L, int i) {
  int res = 0, isnum = 0;
  lj_gc_check(L);
  lua_len(L, i);
  res = (int)lua_tointegerx(L, -1, &isnum);
  lua_pop(L, 1);
  if (!isnum)
      lj_err_callerv(L, LJ_ERR_BADLENNUM, lua_typename(L, lua_type(L, i)));
  return res;
}

LUALIB_API const char *luaL_tolstring (lua_State *L, int idx, size_t *len) {
  if (!luaL_callmeta(L, idx, "__tostring")) {  /* no metafield? */
    switch (lua_type(L, idx)) {
      case LUA_TNUMBER:
      case LUA_TSTRING:
        lua_pushvalue(L, idx);
        break;
      case LUA_TBOOLEAN:
        lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
        break;
      case LUA_TNIL:
        lua_pushliteral(L, "nil");
        break;
      default:
        lua_pushfstring(L, "%s: %p", lua_typename(L, idx), lua_topointer(L, idx));
        break;
    }
  }
  return lua_tolstring(L, -1, len);
}

/* -- Coroutine yield and resume ------------------------------------------ */

LUA_API int lua_isyieldable(lua_State *L)
{
  return cframe_canyield(L->cframe);
}

LUA_API int lua_yield(lua_State *L, int nresults)
{
  void *cf = L->cframe;
  global_State *g = G(L);
  if (cframe_canyield(cf)) {
    cf = cframe_raw(cf);
    if (!hook_active(g)) {  /* Regular yield: move results down if needed. */
      cTValue *f = L->top - nresults;
      if (f > L->base) {
	TValue *t = L->base;
	while (--nresults >= 0) copyTV(L, t++, f++);
	L->top = t;
      }
      L->cframe = NULL;
      L->status = LUA_YIELD;
      return -1;
    } else {  /* Yield from hook: add a pseudo-frame. */
      TValue *top = L->top;
      hook_leave(g);
      (top++)->u64 = cframe_multres(cf);
      setcont(top, lj_cont_hook);
      if (LJ_FR2) top++;
      setframe_pc(top, cframe_pc(cf)-1);
      if (LJ_FR2) top++;
      setframe_gc(top, obj2gco(L), LJ_TTHREAD);
      setframe_ftsz(top, ((char *)(top+1)-(char *)L->base)+FRAME_CONT);
      L->top = L->base = top+1;
#if LJ_TARGET_X64
      lj_err_throw(L, LUA_YIELD);
#else
      L->cframe = NULL;
      L->status = LUA_YIELD;
      lj_vm_unwind_c(cf, LUA_YIELD);
#endif
    }
  }
  lj_err_msg(L, LJ_ERR_CYIELD);
  return 0;  /* unreachable */
}

LUA_API int lua_resume(lua_State *L, int nargs)
{
  if (L->cframe == NULL && L->status <= LUA_YIELD)
    return lj_vm_resume(L,
      L->status == LUA_OK ? api_call_base(L, nargs) : L->top - nargs,
      0, 0);
  L->top = L->base;
  setstrV(L, L->top, lj_err_str(L, LJ_ERR_COSUSP));
  incr_top(L);
  return LUA_ERRRUN;
}


LUA_API void lua_setlevel(lua_State *from, lua_State *to)
{
  UNUSED(from);
  UNUSED(to);
}

/* -- GC and memory management -------------------------------------------- */

LUA_API int lua_gc(lua_State *L, int what, int data)
{
  global_State *g = G(L);
  int res = 0;
  switch (what) {
  case LUA_GCSTOP:
    g->gc.threshold = LJ_MAX_MEM;
    break;
  case LUA_GCRESTART:
    g->gc.threshold = data == -1 ? (g->gc.total/100)*g->gc.pause : g->gc.total;
    break;
  case LUA_GCCOLLECT:
    lj_gc_fullgc(L);
    break;
  case LUA_GCCOUNT:
    res = (int)(g->gc.total >> 10);
    break;
  case LUA_GCCOUNTB:
    res = (int)(g->gc.total & 0x3ff);
    break;
  case LUA_GCSTEP: {
    GCSize a = (GCSize)data << 10;
    g->gc.threshold = (a <= g->gc.total) ? (g->gc.total - a) : 0;
    while (g->gc.total >= g->gc.threshold)
      if (lj_gc_step(L) > 0) {
	res = 1;
	break;
      }
    break;
  }
  case LUA_GCSETPAUSE:
    res = (int)(g->gc.pause);
    g->gc.pause = (MSize)data;
    break;
  case LUA_GCSETSTEPMUL:
    res = (int)(g->gc.stepmul);
    g->gc.stepmul = (MSize)data;
    break;
  case LUA_GCISRUNNING:
    res = (g->gc.threshold != LJ_MAX_MEM);
    break;
  default:
    res = -1;  /* Invalid option. */
  }
  return res;
}

LUA_API lua_Alloc lua_getallocf(lua_State *L, void **ud)
{
  global_State *g = G(L);
  if (ud) *ud = g->allocd;
  return g->allocf;
}

LUA_API void lua_setallocf(lua_State *L, lua_Alloc f, void *ud)
{
  global_State *g = G(L);
  g->allocd = ud;
  g->allocf = f;
}

LUA_API const lua_Number *lua_version (lua_State *L) {
  static const lua_Number version = LUA_VERSION_NUM;
  if (L == NULL) return &version;
  else return G(L)->version;
}

/* XXX TBD these are not good at all. */
LUA_API void  lua_callk(lua_State *L, int nargs, int nresults,
		                           lua_KContext ctx, lua_KFunction k)
{
  lua_call(L, nargs, nresults);
}

LUA_API int lua_pcallk(lua_State *L, int nargs, int nresults, int errfunc,
		                            lua_KContext ctx, lua_KFunction k)
{
  return lua_pcall(L, nargs, nresults, errfunc);
}

LUA_API int  (lua_yieldk)(lua_State *L, int nresults, lua_KContext ctx,
                               lua_KFunction k)
{
  return lua_yield(L, nresults);
}

LUA_API void lua_toclose (lua_State *L, int idx) {
  // TODO
}

