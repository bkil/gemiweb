#define _XOPEN_SOURCE 700 // strdup
#define _POSIX_C_SOURCE 200809L // strdup

#include <string.h> // strdup
#include <stdio.h> // fputs stderr
#include "vm.h"

static int
__attribute__((pure, nonnull))
getCycles(Object *o) {
  if ((o->t != MapObject) && (o->t != ArrayObject) && (o->t != FunctionJs) && (o->t != MethodNative)) {
    return 0;
  } else if (o->ref == -2) {
    return 1;
  } else if (o->ref < 0) {
    return 0;
  }

  int savedRef = o->ref;
  o->ref = -2;
  int c = 0;

  if ((o->t == MapObject) || (o->t == ArrayObject)) {
    List *it = o->V.m;
    while (it) {
      if (getCycles(it->value)) {
        c = 1;
        break;
      }
      it = it->next;
    }
  } else if (o->t == FunctionJs) {
    c = getCycles(o->V.j.scope);
  } else if (o->t == MethodNative) {
    c = getCycles(o->V.a.self);
  }

  o->ref = savedRef;
  return c;
}

static void
List_free(List *list);

static void
__attribute__((nonnull))
Object_free(Object *o) {
  if (o->ref) {
    if (o->ref > 0) {
      o->ref--;
    }
    return;
  }

  if (o->t == StringObject) {
    free(o->V.s.s);
  } else if (o->t == ConstStringObject) {
    if (o->V.c.h) {
      Object_free(o->V.c.h);
    }
  } else if ((o->t == MapObject) || (o->t == ArrayObject)) {
    List_free(o->V.m);
  } else if (o->t == FunctionJs) {
    Object_free(o->V.j.scope);
    Object_free(o->V.j.p.h);
  } else if (o->t == MethodNative) {
    Object_free(o->V.a.self);
  }
  free(o);
}

static Object *
__attribute__((nonnull))
Object_ref(Object *o) {
  if (o->ref >= 0) {
    o->ref++;
  }
  return o;
}

static Object *
__attribute__((nonnull))
Object_clone(Object *o) {
  if (o->t != MapObject) {
    return Object_ref(o);
  }

  List *it = o->V.m;
  Object *d = MapObject_new();
  while (it) {
    d->V.m = List_new(d->V.m, strdup(it->key), Object_ref(it->value));
    it = it->next;
  }
  return d;
}

static void
List_free(List *list) {
  List *n;
  while (list) {
    n = list->next;
    if (list->key) {
      free(list->key);
    }
    if (list->value) {
      Object_free(list->value);
    }
    free(list);
    list = n;
  }
}

void
__attribute__((nonnull))
Parser_free(Parser *p) {
  Object_free(p->vars);
  free(p);
}
