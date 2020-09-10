/*
** Function handling (prototypes, functions and upvalues).
** Copyright (C) 2005-2020 Mike Pall. See Copyright Notice in luajit.h
**
** Upvalue handling rewrite and closure lifting.
** Copyright (C) 2014 Karel Tuma. See Copyright Notice in luajit.h
**
** Portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_func_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_func.h"
#include "lj_trace.h"
#include "lj_vm.h"
#include "lj_tab.h"

/* -- Prototypes ---------------------------------------------------------- */

void LJ_FASTCALL lj_func_freeproto(global_State *g, GCproto *pt)
{
  lj_mem_free(g, pt, pt->sizept);
}

/* -- Upvalues ------------------------------------------------------------ */

static void unlinkuv(global_State *g, GCupval *uv)
{
  UNUSED(g);
  lj_assertG(uvprev(uvnext(uv)) == uv && uvnext(uvprev(uv)) == uv,
	     "broken upvalue chain");
  setgcrefr(uvnext(uv)->prev, uv->prev);
  setgcrefr(uvprev(uv)->next, uv->next);
}

/* Find existing open upvalue for a stack slot or create a new one. */
static GCupval *func_finduv(lua_State *L, TValue *slot)
{
  global_State *g = G(L);
  GCRef *pp = &L->openupval;
  GCupval *p;
  GCupval *uv;
  /* Search the sorted list of open upvalues. */
  while (gcref(*pp) != NULL && uvval((p = gco2uv(gcref(*pp)))) >= slot) {
    lj_assertG(!p->closed && uvval(p) != &p->tv, "closed upvalue in chain");
    if (uvval(p) == slot) {  /* Found open upvalue pointing to same slot? */
      if (isdead(g, obj2gco(p)))  /* Resurrect it, if it's dead. */
	flipwhite(obj2gco(p));
      return p;
    }
    pp = &p->nextgc;
  }
  /* No matching upvalue found. Create a new one. */
  uv = lj_mem_newt(L, sizeof(GCupval), GCupval);
  newwhite(g, uv);
  uv->gct = ~LJ_TUPVAL;
  uv->closed = 0;  /* Still open. */
  setmref(uv->v, slot);  /* Pointing to the stack slot. */
  /* NOBARRIER: The GCupval is new (marked white) and open. */
  setgcrefr(uv->nextgc, *pp);  /* Insert into sorted list of open upvalues. */
  setgcref(*pp, obj2gco(uv));
  setgcref(uv->prev, obj2gco(&g->uvhead));  /* Insert into GC list, too. */
  setgcrefr(uv->next, g->uvhead.next);
  setgcref(uvnext(uv)->prev, obj2gco(uv));
  setgcref(g->uvhead.next, obj2gco(uv));
  lj_assertG(uvprev(uvnext(uv)) == uv && uvnext(uvprev(uv)) == uv,
	     "broken upvalue chain");
  return uv;
}

/* Create an empty and closed upvalue. */
static GCupval *func_emptyuv(lua_State *L)
{
  GCupval *uv = (GCupval *)lj_mem_newgco(L, sizeof(GCupval));
  uv->gct = ~LJ_TUPVAL;
  uv->closed = 1;
  setnilV(&uv->tv);
  setmref(uv->v, &uv->tv);
  return uv;
}

/* Close all open upvalues pointing to some stack level or above. */
void LJ_FASTCALL lj_func_closeuv(lua_State *L, TValue *level)
{
  GCupval *uv;
  global_State *g = G(L);
  while (gcref(L->openupval) != NULL &&
	 uvval((uv = gco2uv(gcref(L->openupval)))) >= level) {
    GCobj *o = obj2gco(uv);
    lj_assertG(!isblack(o), "bad black upvalue");
    lj_assertG(!uv->closed && uvval(uv) != &uv->tv, "closed upvalue in chain");
    setgcrefr(L->openupval, uv->nextgc);  /* No longer in open list. */
    if (isdead(g, o)) {
      lj_func_freeuv(g, uv);
    } else {
      unlinkuv(g, uv);
      lj_gc_closeuv(g, uv);
    }
  }
}

void LJ_FASTCALL lj_func_freeuv(global_State *g, GCupval *uv)
{
  if (!uv->closed)
    unlinkuv(g, uv);
  lj_mem_freet(g, uv);
}

/* -- Functions (closures) ------------------------------------------------ */

GCfunc *lj_func_newC(lua_State *L, MSize nelems, GCtab *env)
{
  GCfunc *fn = (GCfunc *)lj_mem_newgco(L, sizeCfunc(nelems));
  fn->c.gct = ~LJ_TFUNC;
  fn->c.ffid = FF_C;
  fn->c.nupvalues = (uint8_t)nelems;
  /* NOBARRIER: The GCfunc is new (marked white). */
  setmref(fn->c.pc, &G(L)->bc_cfunc_ext);
  setgcref(fn->c.env, obj2gco(env));
  return fn;
}

static GCfunc *func_newL(lua_State *L, GCproto *pt, GCobj *env)
{
  uint32_t count;
  GCfunc *fn = (GCfunc *)lj_mem_newgco(L, sizeLfunc((MSize)pt->sizeuv));
  fn->l.gct = ~LJ_TFUNC;
  fn->l.ffid = FF_LUA;
  fn->l.nupvalues = 0;  /* Set to zero until upvalues are initialized. */
  /* NOBARRIER: Really a setgcref. But the GCfunc is new (marked white). */
  setmref(fn->l.pc, proto_bc(pt));
  setgcref(fn->l.env, env);
  /* Saturating 3 bit counter (0..7) for created closures. */
  count = (uint32_t)pt->flags + PROTO_CLCOUNT;
  pt->flags = (uint8_t)(count - ((count >> PROTO_CLC_BITS) & PROTO_CLCOUNT));
  return fn;
}

static GCfunc *lj_func_newL(lua_State *L, GCproto *pt, GCfunc *parent);
/* Recursively instantiate closures */
static inline
void lj_func_init_closure(lua_State *L, uintptr_t i, GCproto *pttab, GCfunc *parent, GCupval *uv)
{
  setfuncV(L, &uv->tv, lj_func_newL(L, &proto_kgc(pttab, (~i))->pt, parent));
}

static GCobj *curr_env(lua_State *L, GCfunc *fn) {
  GCobj *ret = NULL;
  if (isluafunc(fn))
    ret = gcref(fn->l.env);
  if (!ret)
    ret = gcref(L->env);
  return ret;
}

/* Create a new Lua function with empty upvalues. This is called on the top level chunk. */
GCfunc *lj_func_newL_empty(lua_State *L, GCproto *pt, cTValue *env)
{
  if (!env)
    env = lj_tab_getint(tabV(registry(L)), LUA_RIDX_GLOBALS);
  GCfunc *fn = func_newL(L, pt, curr_env(L, curr_func(L)));
  MSize i, nuv = pt->sizeuv;
  for (i = 0; i < nuv; i++) {
    GCupval *uv = func_emptyuv(L);
    uint32_t v = proto_uv(pt)[i];
    uint8_t flags = (v >> PROTO_UV_SHIFT);
/*    if (flags == UV_HOLE) {
      setgcrefnull(fn->l.uvptr[i]);
      continue;
    }*/
    uv->flags = flags;
    uv->dhash = (uint32_t)(uintptr_t)pt ^ ((uint32_t)proto_uv(pt)[i] << 24);
    setgcref(fn->l.uvptr[i], obj2gco(uv));
    lj_gc_objbarrier(L, fn, uv);
    // there's no chained or local
    if (flags & UV_ENV) {
      copyTV(L, &uv->tv, env);
    } else if (flags == UV_CLOSURE)
      lj_func_init_closure(L, v & PROTO_UV_MASK, pt, fn, uv);
  }
  fn->l.nupvalues = (uint8_t)nuv;
  return fn;
}

/* Do a GC check and create a new Lua function with inherited upvalues. This is called for lambdas below top level. */
static GCfunc *lj_func_newL(lua_State *L, GCproto *pt, GCfunc *parent)
{
  GCfunc *fn;
  GCRef *puv;
  MSize i, nuv;
  TValue *base;
  fn = func_newL(L, pt, curr_env(L, parent));
  puv = parent->l.uvptr;
  nuv = pt->sizeuv;
  base = L->base;

  fn->l.nupvalues = 0;
  for (i = 0; i < nuv; i++) {
    uint32_t v = proto_uv(pt)[i];
    GCupval *uv;
    switch (v >> PROTO_UV_SHIFT) {
      case UV_CLOSURE:
        uv = func_emptyuv(L);
        uv->flags = v >> PROTO_UV_SHIFT;
        setgcref(fn->l.uvptr[i], obj2gco(uv));
        lj_gc_objbarrier(L, fn, uv);
        lj_func_init_closure(L, v & PROTO_UV_MASK, pt, fn, uv);
        break;
      case UV_CHAINED:
        uv = &gcref(puv[v & PROTO_UV_MASK])->uv;
        setgcref(fn->l.uvptr[i], obj2gco(uv));
        lj_gc_objbarrier(L, fn, uv);
        break;
      case UV_LOCAL|UV_IMMUTABLE:
      case UV_LOCAL:
        uv = func_finduv(L, base + (v & 0xff));
        uv->flags = v >> PROTO_UV_SHIFT;
        uv->dhash = (uint32_t)(uintptr_t)mref(parent->l.pc, char) ^ (v << 24);
        setgcref(fn->l.uvptr[i], obj2gco(uv));
        lj_gc_objbarrier(L, fn, uv);
        break;
      default:
        lj_assertL(0, "invalid uvproto flags");
    }
    fn->l.nupvalues = (uint8_t)nuv;
  }
  return fn;
}

void ljx_func_setfenv(lua_State *L, GCfunc *fn, GCtab *t)
{
  if (isluafunc(fn)) {
    GCupval *uv = func_emptyuv(L);
    settabV(L, &uv->tv, t);
    lj_gc_objbarrier(L, uv, t);
    int nup = fn->l.nupvalues;
    for (int i = 0; i < nup; i++) {
      GCupval *uvo = gco2uv(gcref(fn->l.uvptr[i]));
      if (uvo->flags == UV_CLOSURE) {
        GCfunc *subfn = gco2func(obj2gco(uvval(uvo)));
        setgcref(subfn->l.env, obj2gco(t));
        lj_gc_objbarrier(L, fn, t);
      }
    }
    setgcref(fn->l.env, obj2gco(t));
  } else setgcref(fn->c.env, obj2gco(t));
  lj_gc_objbarrier(L, fn, t);
}
  
GCfunc *lj_func_newL_gc(lua_State *L, GCproto *pt, GCfuncL *parent)
{
  lj_gc_check_fixtop(L);
  return lj_func_newL(L, pt, (GCfunc*)parent);
}

cTValue * LJ_FASTCALL lj_curr_env(lua_State *L, int searchuv)
{
  GCfunc *fn = curr_func(L);
  GCobj *gco = curr_env(L, fn);
  if (gco) {
    TValue *o = &G(L)->tmptv;
    settabV(L, o, gco2tab(gco));
    return o;
  } else if (searchuv && isluafunc(fn)) {
    for (int i = 0; i < fn->l.nupvalues; i++) {
      GCupval *uvo = gco2uv(gcref(fn->l.uvptr[i]));
      if (uvo->flags == UV_ENV)
        return uvval(uvo);
    }
  }
  return lj_tab_getint(tabV(registry(L)), LUA_RIDX_GLOBALS);
}

void LJ_FASTCALL lj_func_free(global_State *g, GCfunc *fn)
{
  MSize size = isluafunc(fn) ? sizeLfunc((MSize)fn->l.nupvalues) :
			       sizeCfunc((MSize)fn->c.nupvalues);
  lj_mem_free(g, fn, size);
}

