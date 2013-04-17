/*
** $Id: lstring.c,v 2.8.1.1 2007/12/27 13:02:25 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* for uintptr_t */

#include "gc.h"

#define lstring_c
#define LUA_CORE

#include "lua.h"

#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"

#define HIDE(p) (void*)(~((uintptr_t)p))
#define UNHIDE(p) HIDE(p)

#define hidestr(p) (TString*)HIDE(p)
#define unhidestr(p) (TString*)(GC_call_with_alloc_lock(unhidestr_impl, p))

static void *unhidestr_impl(void *p) {
    SChain *cell = (SChain*)p;
    if (cell->str == NULL) {
        return NULL;
    } else {
        return UNHIDE(cell->str);
    }
}

/*
static int check_stringtable(stringtable *tb) {
    int nuse = tb->nuse;
    int cnt = 0;
    int i, j;
    SChain *cell;
    const char **strings = (const char**)malloc(sizeof(char*) * nuse);
    int *lens = (int*)malloc(sizeof(int) * nuse);
    for (i = 0; i < tb->size; i++) {
        cell = tb->hash[i];
        while (cell != NULL) {
            TString *ts = unhidestr(cell);
            if (ts != NULL) {
                const char *str = getstr(ts);
                int len = ts->tsv.len;
                for (j = 0; j < cnt; j++) {
                    const char *str1 = strings[j];
                    int len1 = lens[j];
                    if (str1 != NULL) {
                        if (len == len1 && memcmp(str, str1, len) == 0) {
                            fprintf(stderr, "String %s in string table twice!\n", str);
                            exit(1);
                        }
                    } else {
                        if (len1 != -1) {
                            fprintf(stderr, "strings and lens inconsistent\n");
                            exit(1);
                        }
                    }
                }
                strings[cnt] = str;
                lens[cnt] = len;
            } else {
                strings[cnt] = NULL;
                lens[cnt] = -1;
            }
            cnt++;
            if (cnt > nuse) {
                fprintf(stderr, "cnt (%d) > nuse (%d)\n", cnt, nuse);
                exit(1);
            }
            cell = cell->next;
        }
    }
    if (cnt != nuse) {
        fprintf(stderr, "cnt (%d) != nuse (%d)\n", cnt, nuse);
        exit(1);
    }
    free(strings);
    free(lens);
}
*/

static void remove_dead_strings (stringtable *tb) {
    SChain **prev;
    SChain *cell;
    int i;
    for (i = 0; i < tb->size; i++) {
        prev = &tb->hash[i];
        cell = *prev;
        while (cell != NULL) {
            TString *ts = unhidestr(cell);
            if (ts == NULL) {
                /* cell->str collected, so remove */
                *prev = cell->next;
                tb->nuse--;
            } else {
                prev = &cell->next;
            }
            cell = cell->next;
        }
    }
}

void luaS_fix (lua_State *L, TString *ts) {
    if (G(L)->nfixedstrs >= LUA_MAX_FIXED_STRINGS) {
        fprintf(stderr, "Too many fixed strings\n");
        exit(EXIT_FAILURE);
    }
    G(L)->fixedstrs[G(L)->nfixedstrs] = ts;
    G(L)->nfixedstrs++;
}

void luaS_resize (lua_State *L, int newsize) {
  SChain **newhash;
  TString *ts;
  stringtable *tb;
  int i;
  newhash = luaM_newvector(L, newsize, SChain *);
  tb = &G(L)->strt;
  for (i=0; i<newsize; i++) newhash[i] = NULL;
  /* rehash */
  for (i=0; i<tb->size; i++) {
    SChain *p = tb->hash[i];
    while (p) {  /* for each node in the list */
      SChain *next = p->next;  /* save next */
      ts = unhidestr(p);
      if (ts == NULL) {
          /* collected, so skip */
          tb->nuse--;
      } else {
          unsigned int h = ts->tsv.hash;
          int h1 = lmod(h, newsize);  /* new position */
          lua_assert(cast_int(h%newsize) == lmod(h, newsize));
          p->next = newhash[h1];  /* chain it */
          newhash[h1] = p;
      }
      p = next;
    }
  }
  tb->size = newsize;
  tb->hash = newhash;
}

static TString *newlstr (lua_State *L, const char *str, size_t l,
                                       unsigned int h) {
  TString *ts;
  SChain *cell;
  stringtable *tb = &G(L)->strt;
  if (l+1 > (MAX_SIZET - sizeof(TString))/sizeof(char))
    luaM_toobig(L);
  ts = cast(TString *, luaM_malloc_atomic((l+1)*sizeof(char)+sizeof(TString)));
  cell = cast(SChain *, luaM_malloc(sizeof(SChain))); 
  cell->str = hidestr(ts);
  luaM_register_disappearing_link((void**)&cell->str, ts);

  ts->tsv.len = l;
  ts->tsv.hash = h;
#ifdef LUA_DEBUG
  ts->tsv.tt = LUA_TSTRING;
#endif
  ts->tsv.reserved = 0;
  memcpy(ts+1, str, l*sizeof(char));
  ((char *)(ts+1))[l] = '\0';  /* ending 0 */
  h = lmod(h, tb->size);
  cell->next = tb->hash[h];  /* chain new entry */
  tb->hash[h] = cell;
  tb->nuse++;
  if (tb->nuse > cast(lu_int32, tb->size) && tb->size <= MAX_INT/2) {
      /* too crowded, try removing dead strings */
      remove_dead_strings(tb);
      if (tb->nuse > cast(lu_int32, tb->size) && tb->size <= MAX_INT/2) {
         luaS_resize(L, tb->size*2);  /* still too crowded, resize */
      }
  }
  /* check_stringtable(tb); */
  return ts;
}


TString *luaS_newlstr (lua_State *L, const char *str, size_t l) {
  SChain *cell;
  TString *ts;
  stringtable *tb;
  unsigned int h = cast(unsigned int, l);  /* seed */
  unsigned int h1;
  size_t step = (l>>5)+1;  /* if string is too long, don't hash all its chars */
  size_t l1;
  for (l1=l; l1>=step; l1-=step)  /* compute hash */
    h = h ^ ((h<<5)+(h>>2)+cast(unsigned char, str[l1-1]));
  h1 = lmod(h, G(L)->strt.size);
  tb = &G(L)->strt;
  for (cell = tb->hash[h1]; cell != NULL; cell = cell->next) {

    ts = unhidestr(cell);
    if (ts == NULL) { 
        /* string collected, skip */
    } else {
        if (ts->tsv.len == l && (memcmp(str, getstr(ts), l) == 0)) {
          return ts;
        }
    }
  }

  ts = newlstr(L, str, l, h);  /* not found */
  return ts;
}


Udata *luaS_newudata (lua_State *L, size_t s, Table *e) {
  Udata *u;
  if (s > MAX_SIZET - sizeof(Udata))
    luaM_toobig(L);
  u = cast(Udata *, luaM_malloc(s + sizeof(Udata)));
#ifdef LUA_DEBUG
  u->uv.tt = LUA_TUSERDATA;
#endif
  u->uv.len = s;
  u->uv.metatable = NULL;
  u->uv.env = e;
  /* chain it on udata list (after main thread) */
  return u;
}

