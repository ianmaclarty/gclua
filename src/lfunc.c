/*
** $Id: lfunc.c,v 2.12.1.2 2007/12/28 14:58:43 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/


#include <stddef.h>

#define lfunc_c
#define LUA_CORE

#include "lua.h"

#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



Closure *luaF_newCclosure (lua_State *L, int nelems, Table *e) {
  Closure *c = cast(Closure *, luaM_malloc(sizeCclosure(nelems)));
#ifdef LUA_DEBUG
  c->c.tt = LUA_TFUNCTION;
#endif
  c->c.isC = 1;
  c->c.env = e;
  c->c.nupvalues = cast_byte(nelems);
  return c;
}


Closure *luaF_newLclosure (lua_State *L, int nelems, Table *e) {
  Closure *c = cast(Closure *, luaM_malloc(sizeLclosure(nelems)));
#ifdef LUA_DEBUG
  c->l.tt = LUA_TFUNCTION;
#endif
  c->l.isC = 0;
  c->l.env = e;
  c->l.nupvalues = cast_byte(nelems);
  while (nelems--) c->l.upvals[nelems] = NULL;
  return c;
}

UpVal *luaF_newupval (lua_State *L) {
  UpVal *uv = luaM_new(L, UpVal);
#ifdef LUA_DEBUG
  uv->tt = LUA_TUPVAL;
#endif
  uv->uvnext = NULL;  /* luaF_newupval  is only called in ldo.c
                       * when deserializing a dumped closure with upvalues.
                       * The resulting upvalues will be closed, so uvnext will
                       * not be used.
                       */
  uv->v = &uv->value;
  setnilvalue(uv->v);
  return uv;
}


UpVal *luaF_findupval (lua_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  UpVal *uv;
  while (*pp != NULL && (p = *pp)->v >= level) {
    lua_assert(p->v != &p->value);
    if (p->v == level) {  /* found a corresponding upvalue? */
      return p;
    }
    pp = &p->uvnext;
  }
  uv = luaM_new(L, UpVal);  /* not found: create a new one */
#ifdef LUA_DEBUG
  uv->tt = LUA_TUPVAL;
#endif
  uv->v = level;  /* current value lives in the stack */
  uv->uvnext = *pp;  /* chain it in the proper position */
  *pp = uv;
  return uv;
}

void luaF_close (lua_State *L, StkId level) {
  UpVal *uv;
  while (L->openupval != NULL && (uv = L->openupval)->v >= level) {
    lua_assert(uv->v != &uv->value);
    L->openupval = uv->uvnext;  /* remove from `open' list */
    setobj(L, &uv->value, uv->v);
    uv->v = &uv->value;  /* now current value lives here */
  }
}


Proto *luaF_newproto (lua_State *L) {
  Proto *f = luaM_new(L, Proto);
#ifdef LUA_DEBUG
  f->tt = LUA_TPROTO;
#endif
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->sizecode = 0;
  f->sizelineinfo = 0;
  f->sizeupvalues = 0;
  f->nups = 0;
  f->upvalues = NULL;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->lineinfo = NULL;
  f->sizelocvars = 0;
  f->locvars = NULL;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}


/*
** Look for n-th local variable at line `line' in function `func'.
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}

