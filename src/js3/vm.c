/* Copyright (C) 2023 bkil.hu
Refer to the GNU GPL v2 in LICENSE for terms */

#include "include.h" /* strnlen strdup strndup getline clock_gettime; strlen strcmp strncmp strncpy strstr */
#include "vm-impl.h"

#include <stdio.h> /* STDIN_FILENO fputc fputs getline perror putchar stderr stdout */
#include <malloc.h> /* free malloc */
#include <stdlib.h> /* atoi size_t */
#include <sys/mman.h> /* mmap */
#include <sys/types.h> /* fstat open opendir regcomp regexec regfree select */
#include <regex.h> /* regcomp regexec regfree */
#include <sys/stat.h> /* fstat open */
#include <fcntl.h> /* open */
#include <unistd.h> /* close fstat read select write */
#include <sys/time.h> /* select */
#include <limits.h> /* INT_MAX INT_MIN */
#include <stddef.h> /* offsetof */
#include <dirent.h> /* closedir opendir readdir */
#include <errno.h> /* errno */

/* coverage:stderr */
static void
__attribute__((nonnull))
putsn(const char *s, size_t len, FILE *out) {
  while (len-- && *s) {
    fputc(*(s++), out);
  }
}

static void
__attribute__((nonnull))
showProg(Parser *p) {
  const size_t left = off_t2size_t(p->prog.end - p->prog.s);
  putsn(p->prog.s, left < 32 ? left : 32, stderr);
  fputs("\n", stderr);
}
/* /coverage:stderr */

#ifdef SMALLBIN
# include "vm-smallbin.c"

#else

# include <stdio.h> /* snprintf */

static int
__attribute__((nonnull))
snprinti(char *s, size_t n, int i) {
  return snprintf(s, n, "%d", i);
}
#endif

#ifdef NEEDLEAK
# define getCycles(o) (0)
# define Object_ref(o) (o)
# define Object_clone(o) (o)
# define mfree(o) while(0) {}
# define Object_free(o) while(0) {}
# define Object_freeMaybe(o) while(0) {}
# define List_free(l) while(0) {}

void
__attribute__((const))
Parser_free(Parser *p) {
}

# else

static Object *MapObject_new(void);
List *List_new(List *next, char *key, Object *value);
# define mfree(o) free(o)

# include "vm-free.c"
#endif

static void
__attribute__((nonnull))
clearErr(Parser *p) {
  p->parseErr = 0;
  p->parseErrChar = 0;
}

static Object *
__attribute__((nonnull(1)))
setRunError(Parser *p, const char *message, const Id *id) {
  p->err = message;
  p->errName = id ? *id : (Id){0};
  return 0;
}

#ifdef NEEDLEAK
static
#endif
List *
__attribute__((malloc, returns_nonnull, warn_unused_result, nonnull(3)))
List_new(List *next, char *key, Object *value) {
  List *l = malloc(sizeof(*l));
  l->next = next;
  l->key = key;
  l->value = value;
  return l;
}

static Object undefinedObject = {.ref = -1, .t = UndefinedObject, .V.i = 0};
static Object nullObject = {.ref = -1, .t = NullObject, .V.i = 0};
static Object nanObject = {.ref = -1, .t = NanObject, .V.i = 0};
static Object emptyString = {.ref = -1, .t = ConstStringObject, .V.c = {.len = 0, .s = ""}};

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
IntObject_new(int x) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.i));
  o->ref = 0;
  o->t = IntObject;
  o->V.i = x;
  return o;
}

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
StringObject_new_str(Str s) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.s));
  o->ref = 0;
  o->t = StringObject;
  o->V.s = s;
  return o;
}

/* coverage:file */
static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
StringObject_new_dup(const char *s) {
  return StringObject_new_str((Str){.s = strdup(s), .len = strlen(s)});
}
/* /coverage:file */

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
StringObject_new_const(Id s) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.c));
  o->ref = 0;
  o->t = ConstStringObject;
  o->V.c = (Id){.s = s.s, .len = s.len, .h = s.h ? Object_ref(s.h) : 0};
  return o;
}

/* coverage:file */
static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
Mmap_new(char *s, size_t len, int fd) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.mm));
  o->ref = 0;
  o->t = MmapString;
  o->V.mm.s = s;
  o->V.mm.len = len;
  o->V.mm.fd = fd;
  return o;
}
/* /coverage:file */

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result, nonnull))
StringObject_new(const char *s) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.c));
  o->ref = 0;
  o->t = ConstStringObject;
  o->V.c = (Id){.s = s, .len = strlen(s), .h = 0};
  return o;
}

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
StringObject_new_char(char c) {
  Str s = {.s = 0, .len = 1};
  s.s = malloc(2);
  s.s[0] = c;
  s.s[1] = 0;
  return StringObject_new_str(s);
}

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result, nonnull))
FunctionJs_new(Parser *p) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.j));
  o->ref = 0;
  o->t = FunctionJs;
  o->V.j = (JsFun){.p = {.s = p->prog.s, .end = p->prog.end, .h = Object_ref(p->prog.h)}, .scope = Object_clone(p->vars)};
  return o;
}

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
FunctionNative_new(Native f) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.f));
  o->ref = 0;
  o->t = FunctionNative;
  o->V.f = f;
  return o;
}

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
MethodNative_new(MethodFun a) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.a));
  o->ref = 0;
  o->t = MethodNative;
  o->V.a = a;
  return o;
}

static
Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
MapObject_new(void) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.m));
  o->ref = 0;
  o->t = MapObject;
  o->V.m = 0;
  return o;
}

static
Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
Prototype_new(void) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.m));
  o->ref = 0;
  o->t = Prototype;
  o->V.m = 0;
  return o;
}

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
ArrayObject_new(void) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.m));
  o->ref = 0;
  o->t = ArrayObject;
  o->V.m = 0;
  return o;
}

static void
__attribute__((nonnull(1)))
Object_setUnref(Object **old, Object *next) {
  Object_freeMaybe(*old);
  *old = next;
}

static void
__attribute__((nonnull))
Object_set(Object **old, Object *next) {
  Object_setUnref(old, Object_ref(next));
}

static void
__attribute__((nonnull))
Object_set0(Object **old) {
  Object_setUnref(old, 0);
}

static Object *
__attribute__((malloc, returns_nonnull, warn_unused_result))
DateObject_new(long d) {
  Object *o = malloc(offsetof(Object, V) + sizeof(o->V.d));
  o->ref = 0;
  o->t = DateObject;
  o->V.d = d;
  return o;
}

static Object *
__attribute__((nonnull, warn_unused_result))
DateObject_now(Parser *p) {
  struct timespec tp;
  if (!clock_gettime(CLOCK_REALTIME, &tp)) {
    return DateObject_new(tp.tv_sec * 1000 + tp.tv_nsec / 1000000);
  }
  /* coverage:no */
  return setRunError(p, "clock_gettime() failed", 0);
  /* /coverage:no */
}

static void
__attribute__((nonnull(2, 3)))
Map_set(List **list, char *key, Object *value) {
  if (!list) {
    return;
  }
  List *it = *list;
  while (it && it->key) {
    if (!strcmp(key, it->key)) {
      Object_set(&it->value, value);
      mfree(key);
      return;
    }
    it = it->next;
  }
  *list = List_new(*list, key, Object_ref(value));
}

static void
__attribute__((nonnull(2, 3)))
Map_set_const(List **list, const char *s, Object *value) {
  Map_set(list, strdup(s), value);
}

static void
__attribute__((nonnull(2, 3)))
Map_set_id(List **list, Id *id, Object *value) {
  Map_set(list, strndup(id->s, id->len), value);
}

static int
__attribute__((pure, nonnull, warn_unused_result))
strncmpEq(Id id, const char *s) {
  return !strncmp(id.s, s, id.len) && !s[id.len];
}

static List *
__attribute__((pure, warn_unused_result))
Map_get(List *list, Id id) {
  List *it = list;
  while (it) {
    if (it->key) {
      if (strncmpEq(id, it->key)) {
        return it;
      }
    } else {
      List *sub = Map_get(it->value->V.m, id);
      if (sub) {
        return sub;
      }
    }
    it = it->next;
  }

  if (!id.len) {
    return 0;
  }
  it = Map_get(list, (Id){.s = "", .len = 0});
  if (!it || (it->value->t != MapObject)) {
    return 0;
  }
  return Map_get(it->value->V.m, id);
}

static List *
__attribute__((pure, warn_unused_result))
Map_get_str(List *list, Str s) {
  return Map_get(list, (Id){.s = s.s, .len = s.len});
}

static List *
__attribute__((pure, warn_unused_result))
Map_get_const(List *list, const char *s) {
  return Map_get(list, (Id){.s = s, .len = strlen(s)});
}

static int
__attribute__((pure, warn_unused_result))
List_length(List *list) {
  int n = 0;
  for (; list; list = list->next) {
     if ((list->key[0] >= '0') && (list->key[0] <= '9')) {
       n++;
     }
  }
  return n;
}

static int
__attribute__((pure, nonnull, warn_unused_result))
isString(Object *o) {
  return (o->t == StringObject) || (o->t == ConstStringObject) || (o->t == MmapString);
}

static int
__attribute__((pure, nonnull, warn_unused_result))
isStringEq(Str a, Str b) {
  return (a.len == b.len) && (!strncmp(a.s, b.s, a.len));
}

static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1, 2)))
String_indexOf(Parser *p, Object *self, List *l) {
  Object *a = l ? l->value : &undefinedObject;
  if (!isString(a)) {
    return &undefinedObject;
  }
  Object *b = l->next ? l->next->value : &undefinedObject;
  size_t pos = (b->t == IntObject) && (b->V.i >= 0) ? (size_t)b->V.i : 0;
  if (pos > self->V.c.len) {
    pos = self->V.c.len;
  }
  char *haystack = strndup(self->V.c.s, self->V.c.len);
  char *needle = strndup(a->V.c.s, a->V.c.len);
  char *start = strstr(haystack + pos, needle);
  int index = start ? off_t2int(start - haystack + 1) - 1 : -1;
  mfree(haystack);
  mfree(needle);
  return IntObject_new(index);
}

static void
__attribute__((nonnull))
List_push(Parser *p, List ***tail, Object *it, Object *value);

static Object *
__attribute__((nonnull, warn_unused_result))
String_substring(Object *o, int start, int end) {
  if (!isString(o)) {
    return 0;
  }
  /* coverage:no */
  if (end > (int)o->V.s.len) {
    end = (int)o->V.s.len;
  }
  if (start < 0) {
    start = 0;
  }
  if (start > end) {
    start = end;
  }
  /* /coverage:no */
  size_t n = (size_t)end - (size_t)start;
  char *s = malloc(n + 1);
  strncpy(s, o->V.s.s + start, n);
  s[n] = 0;
  return StringObject_new_str((Str){.s = s, .len = n});
}

static Object *
__attribute__((nonnull, returns_nonnull, warn_unused_result))
setThrow(Parser *p, const char *message);

static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1, 2)))
String_match(Parser *p, Object *self, List *l) {
  if (!l || !isString(l->value)) {
    return setThrow(p, "expecting String argument");
  }
  regex_t preg;
  char *reg = strndup(l->value->V.s.s, l->value->V.s.len);
  if (regcomp(&preg, reg, REG_EXTENDED | REG_NEWLINE)) {
    mfree(reg);
    return setThrow(p, "expecting valid regexp");
  }
  mfree(reg);

  char *ins = strndup(self->V.s.s, self->V.s.len);
  size_t nmatch = 256;
  regmatch_t m[nmatch];
  if (regexec(&preg, ins, nmatch, m, 0)) {
    mfree(ins);
    regfree(&preg);
    return &nullObject;
  }
  mfree(ins);

  Object *o = ArrayObject_new();
  Object *i = IntObject_new(0);
  List **tail = &o->V.m;
  for (int j = 0; (m[j].rm_so >= 0) && (m[j].rm_eo >= 0); j++) {
    Object *sub = String_substring(self, m[j].rm_so, m[j].rm_eo);
    List_push(p, &tail, i, sub);
  }
  Object_free(i);

  i = IntObject_new(m[0].rm_so);
  Map_set_const(&o->V.m, "index", i);
  Object_free(i);

  regfree(&preg);
  return o;
}

static char
__attribute__((pure, nonnull, warn_unused_result))
String_getCharCode(Object *self, Object *x) {
  int i = x->t == IntObject ? x->V.i : 0;
  return (i >= 0) && (i < (int)self->V.c.len) ? self->V.c.s[i] : -1;
}

static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1, 2)))
String_charCodeAt(Parser *p, Object *self, List *l) {
  char c = String_getCharCode(self, l ? l->value : &undefinedObject);
  return c < 0 ? &nanObject : IntObject_new(c);
}

static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull))
String_charAt_obj(Object *self, Object *x) {
  char c = String_getCharCode(self, x);
  return c < 0 ? &emptyString : StringObject_new_char(c);
}

static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1, 2)))
String_charAt(Parser *p, Object *self, List *l) {
  return String_charAt_obj(self, l ? l->value : &undefinedObject);
}

static int
__attribute__((pure, nonnull, warn_unused_result))
toBoolean(Object *o) {
  switch (o->t) {
    case IntObject:
      return o->V.i;

    case StringObject:
    case ConstStringObject:
    case MmapString:
      return !!o->V.c.len;

    case MapObject:
    case ArrayObject:
    case FunctionJs:
    case FunctionNative:
    case MethodNative:
    case Prototype:
    case DateObject:
      return 1;

    case UndefinedObject:
    case NullObject:
    case NanObject:
    default:
      return 0;
  }
}

static Object *
__attribute__((nonnull, warn_unused_result))
String_concat(Object *t1, Object *t2) {
  if (!isString(t1) || !isString(t2)) {
    return 0;
  }
  const size_t n = t1->V.s.len;
  const size_t m = t2->V.s.len;
  char *s = malloc(n + m + 1);
  strncpy(s, t1->V.s.s, n);
  strncpy(s + n, t2->V.s.s, m);
  s[n + m] = 0;
  return StringObject_new_str((Str){.s = s, .len = n + m});
}

static Object *
setThrow(Parser *p, const char *message) {
  clearErr(p);
  Object_setUnref(&p->thrw, StringObject_new(message));
  Object *part = StringObject_new(" (");
  Object_setUnref(&p->thrw, String_concat(p->thrw, part));

  size_t left = off_t2size_t(p->prog.end - p->prog.s);
  left = left < 32 ? left : 32;
  Object_setUnref(&part, StringObject_new_str((Str){.s = strndup(p->prog.s, left), .len = left}));

  Object_setUnref(&p->thrw, String_concat(p->thrw, part));
  Object_setUnref(&part, StringObject_new("...)"));
  Object_setUnref(&p->thrw, String_concat(p->thrw, part));
  Object_free(part);
  p->nest++;
  return &undefinedObject;
}

static Object *
__attribute__((nonnull, warn_unused_result))
Object_toString(Parser *p, Object *o) {
  switch (o->t) {
    case IntObject: {
      char *s = malloc(16);
      int len = snprinti(s, 16, o->V.i);
      if ((len > 0) && (len < 16)) {
        return StringObject_new_str((Str){.s = s, .len = (size_t)len});
      } else {
        /* coverage:unreachable */
        mfree(s);
        break;
        /* /coverage:unreachable */
      }
    }

    case UndefinedObject:
      return StringObject_new("undefined");

    case StringObject:
    case ConstStringObject:
    case MmapString:
      return Object_ref(o);

    case MapObject:
    case Prototype:
      return StringObject_new("[object Object]");

    case ArrayObject:
    case FunctionJs:
    case FunctionNative:
    /* coverage:no */
    case MethodNative:
    /* /coverage:no */
    case DateObject:
      return setThrow(p, "toString not available for this type");

    case NullObject:
      return StringObject_new("null");

    case NanObject:
      return StringObject_new("NaN");

    default: {}
  }
  return 0; /* unreachable unless memory corruption */
}

static Object *
__attribute__((nonnull, warn_unused_result))
typeOf(Object *o) {
  switch (o->t) {
    case IntObject:
    case NanObject:
      return StringObject_new("number");

    case UndefinedObject:
      return StringObject_new("undefined");

    case StringObject:
    case ConstStringObject:
    case MmapString:
      return StringObject_new("string");

    case MapObject:
    case ArrayObject:
    case NullObject:
    case DateObject:
    case Prototype:
      return StringObject_new("object");

    case FunctionJs:
    case FunctionNative:
    case MethodNative:
      return StringObject_new("function");

    default:
      return 0; /* unreachable unless memory corruption */
  }
}


static int
__attribute__((nonnull, warn_unused_result))
acceptChar(Parser *p, char c) {
  if ((p->prog.s < p->prog.end) && (*p->prog.s == c)) {
    p->prog.s++;
    return 1;
  }
  return 0;
}

static int
__attribute__((nonnull, warn_unused_result))
expectChar(Parser *p, char c) {
  if (acceptChar(p, c)) {
    return 1;
  }
  p->parseErr = "expected different character";
  p->parseErrChar = c;
  return 0;
}

static int
__attribute__((nonnull))
skipWs(Parser *p) {
  int ret = 0;
  while (1) {
    while ((p->prog.s < p->prog.end) && ((*p->prog.s == ' ') || (*p->prog.s == '\n'))) {
      p->prog.s++;
      ret = 1;
    }
    if ((p->prog.end - p->prog.s < 2) || (*p->prog.s != '/')) {
      break;
    }
    if (p->prog.s[1] == '/') {
      p->prog.s += 2;
      ret = 1;
      while ((p->prog.s < p->prog.end) && (*p->prog.s != '\n')) {
        p->prog.s++;
      }
    } else if (p->prog.s[1] == '*') {
      p->prog.s += 2;
      ret = 1;
      while (p->prog.s < p->prog.end) {
        if ((p->prog.end - p->prog.s >= 2) && (*p->prog.s == '*') && (p->prog.s[1] == '/')) {
          p->prog.s += 2;
          break;
        }
        p->prog.s++;
      }
    } else {
      break;
    }
  }
  return ret;
}

static int
__attribute__((nonnull, warn_unused_result))
acceptWs(Parser *p, char c) {
  skipWs(p);
  return acceptChar(p, c);
}

static int
__attribute__((nonnull, warn_unused_result))
expectWs(Parser *p, char c) {
  skipWs(p);
  return expectChar(p, c);
}

static int
__attribute__((nonnull, warn_unused_result))
parseId(Parser *p, Id *id) {
  skipWs(p);
  const char *s = p->prog.s;
  char c = s < p->prog.end ? *s : 0;
  if ((c == '_') || ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))) {
    do {
      s++;
      c = s < p->prog.end ? *s : 0;
    } while ((c == '_') || ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')));
  }
  size_t len = off_t2size_t(s - p->prog.s);
  if (len) {
    *id = (Id){.s = p->prog.s, .len = len, .h = p->prog.h};
    p->prog.s += len;
    return 1;
  }
  p->parseErr = "expected keyword or identifier";
  return 0;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseIntLit(Parser *p) {
  skipWs(p);

  const char *s = p->prog.s;
  char c = s < p->prog.end ? *s : 0;
  if (c == '-') {
    s++;
    c = s < p->prog.end ? *s : 0;
  }
  const char *digitStart = s;
  while ((c >= '0') && (c <= '9')) {
    s++;
    c = s < p->prog.end ? *s : 0;
  }
  size_t len = off_t2size_t(s - p->prog.s);
  size_t digits = off_t2size_t(s - digitStart);
  if (!digits || (c == '_') || (c == '.') || ((c >= 'A' && (c <= 'Z'))) || ((c >= 'a') && (c <= 'z'))) {
    return 0;
  }

  if (p->nest) {
    p->prog.s = s;
    return &undefinedObject;
  }
  int x;
  if (s < p->prog.end) {
    x = atoi(p->prog.s);
  } else {
    char *t = strndup(p->prog.s, len);
    x = atoi(t);
    mfree(t);
  }
  p->prog.s = s;
  return IntObject_new(x);
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseStringLit(Parser *p) {
  skipWs(p);
  char c;
  if (!acceptWs(p, c = '\'') && !acceptWs(p, c = '"')) {
    return 0;
  }
  Id id = {.s = p->prog.s, .len = 0, .h = p->prog.h};
  while ((p->prog.s < p->prog.end) && (*p->prog.s != c) && (*p->prog.s != '\\')) {
    p->prog.s++;
    id.len++;
  }
  if (!expectWs(p, c)) {
    return 0;
  }
  if (p->nest) {
    return &undefinedObject;
  }
  return StringObject_new_const(id);
}

static Object *
parseExpr(Parser *p);

static Object *
__attribute__((nonnull, warn_unused_result))
parseObjectLiteral(Parser *p) {
  if (!acceptWs(p, '{')) {
    return 0;
  }
  Object *o = MapObject_new();
  while (!acceptWs(p, '}')) {
    Id id;
    Object *keyString = 0;
    if (!parseId(p, &id)) {
      clearErr(p);
      Object *keyObj = parseStringLit(p);
      if (!keyObj) {
        Object_free(o);
        return 0;
      }
      keyString = Object_toString(p, keyObj);
      Object_free(keyObj);
      id = keyString->V.c;
    }
    if (!expectWs(p, ':')) {
      Object_freeMaybe(keyString);
      Object_free(o);
      return 0;
    }
    Object *value = parseExpr(p);
    if (!value) {
      Object_freeMaybe(keyString);
      Object_free(o);
      return 0;
    }
    Map_set_id(&o->V.m, &id, value);
    Object_free(value);
    Object_freeMaybe(keyString);
    if (!acceptWs(p, ',')) {
      if (!expectWs(p, '}')) {
        Object_free(o);
        return 0;
      }
      return o;
    }
  }
  return o;
}

static void
List_push(Parser *p, List ***tail, Object *it, Object *value) {
  Object *key = Object_toString(p, it);
  it->V.i++;
  List *l = List_new(0, strndup(key->V.c.s, key->V.c.len), value);
  Object_free(key);
  List **tailp = *tail;
  if (*tailp) {
    (*tailp)->next = l;
    *tail = &(*tailp)->next;
  } else {
    *tailp = l;
  }
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseArrayLiteral(Parser *p) {
  if (!acceptWs(p, '[')) {
    return 0;
  }
  Object *o = ArrayObject_new();
  Object *i = IntObject_new(0);
  List **tail = &o->V.m;
  while (!acceptWs(p, ']')) {
    Object *value = parseExpr(p);
    if (!value) {
      Object_free(o);
      Object_free(i);
      return 0;
    }
    List_push(p, &tail, i, value);
    if (!acceptWs(p, ',')) {
      if (!expectWs(p, ']')) {
        Object_set0(&o);
      }
      Object_free(i);
      return o;
    }
  }
  Object_free(i);
  return o;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseIndex(Parser *p) {
  skipWs(p);
  if (acceptChar(p, '[')) {
    Object *o = parseExpr(p);
    if (o) {
      if (expectWs(p, ']')) {
        return o;
      }
      Object_free(o);
    }
  } else if (acceptChar(p, '.')) {
    Id id;
    if (parseId(p, &id)) {
      return StringObject_new_const(id);
    }
    p->parseErr = "expecting field name";
  }
  return 0;
}

static Object *
parseBody(Parser *p);

static Object *
parseBlock(Parser *p);

static Object *
__attribute__((nonnull(1), warn_unused_result))
parseFunction(Parser *p, List *arg) {
  if (!acceptWs(p, ')')) {
    Id id;
    do {
      if (!parseId(p, &id)) {
        return 0;
      }
      if (!p->nest) {
        if (arg) {
          Map_set_id(&p->vars->V.m, &id, arg->value);
          arg = arg->next;
        } else {
          Map_set_id(&p->vars->V.m, &id, &undefinedObject);
        }
      }
    } while (acceptWs(p, ','));
    if (!expectWs(p, ')')) {
      return 0;
    }
  }
  return parseBody(p);
}

static Object *
__attribute__((nonnull(1, 2), warn_unused_result))
invokeFun(Parser *p, Object *o, List *args, Object *self) {
  if (o->t == FunctionJs) {
    Prog ret = p->prog;
    p->prog = o->V.j.p;
    Object *caller = p->vars;
    p->vars = MapObject_new();
    if (self) {
      Map_set_const(&p->vars->V.m, "this", self);
    }
    Map_set_const(&p->vars->V.m, "", o->V.j.scope);

    Object *res = parseFunction(p, args);
    Object_setUnref(&p->vars, caller);

    if (p->ret) {
      res = p->ret;
      p->ret = 0;
    }
    if (res) {
      p->prog = ret;
      return res;
    }
  } else if (o->t == FunctionNative) {
    return (*o->V.f)(p, args);
  } else if ((o->t == MethodNative) && self) {
    return (*o->V.a)(p, self, args);
  } else {
    return setThrow(p, "not a function");
  }
  return 0;
}

static Object *
__attribute__((nonnull(1), warn_unused_result))
parseRHS(Parser *p, List **parent, Object *key, List *e, Object *got, Object *self) {
  skipWs(p);

  const char *prog = p->prog.s;
  if (acceptChar(p, '=') && !acceptChar(p, '=')) {
    if (!key) {
      return setRunError(p, "expected a writable left hand side", 0);
    }
    if (!p->nest && e && (e->value->t == Prototype)) {
      return setRunError(p, "overwriting prototype unsupported", 0);
    }
    Object *r;
    if ((r = parseExpr(p))) {
      if (!p->nest) {
        if (e) {
          Object_set(&e->value, r);
        } else {
          *parent = List_new(*parent, strndup(key->V.s.s, key->V.s.len), Object_ref(r));
        }
        if (getCycles(r)) {
          if (e) {
            e->value = &undefinedObject;
          } else {
            (*parent)->value = &undefinedObject;
          }
          Object_free(r);
          Object_free(r);
          return setThrow(p, "reference cycles unsupported");
        }
      }
    }
    return r;
  }
  p->prog.s = prog;

  Object *o = Object_ref(e ? e->value : got ? got : &undefinedObject);
  List *args = 0, *argsEnd = 0;
  if (acceptChar(p, '(')) {
    if (!acceptWs(p, ')')) {
      do {
        Object *arg = parseExpr(p);
        if (!arg) {
          Object_free(o);
          List_free(args);
          return 0;
        }
        if (p->nest) {
          Object_free(arg);
        } else {
          List *l = List_new(0, 0, arg);
          if (argsEnd) {
            argsEnd->next = l;
          } else {
            args = l;
          }
          argsEnd = l;
        }
      } while (acceptWs(p, ','));
      if (!expectWs(p, ')')) {
        Object_free(o);
        List_free(args);
        return 0;
      }
    }
    if (p->nest) {
      Object_free(o);
      List_free(args);
      return &undefinedObject;
    }
    Object *f = o;
    o = invokeFun(p, f, args, self);
    Object_free(f);
    List_free(args);
  }

  return o;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseSTerm(Parser *p, Id *id) {
  List **parent = &p->vars->V.m;
  Object *key = &undefinedObject;
  List *e = 0;
  Object *field = 0;
  Object *self = 0;

  if (p->nest) {
    parent = 0;
  } else if (!(e = Map_get(*parent, *id))) {
    return setRunError(p, "undefined variable", id);
  }

  Object *i;
  while ((i = parseIndex(p))) {
    if (p->nest) {
      Object_free(i);
    } else {
      Object_set0(&key);
      if (e) {
        Object_set(&field, e->value);
      }
      if (!field) {
        Object_free(i);
        return setThrow(p, "undefined object field");
      }
      self = field;
      field = 0;
      if (isString(self)) {
        if (isString(i)) {
          if (strncmpEq(i->V.c, "length")) {
            field = IntObject_new((int)self->V.c.len);
            Object_set0(&self);
          } else {
            e = Map_get_str(p->stringPrototype->V.m, i->V.s);
            if (e) {
              field = Object_ref(e->value);
            } else {
              Object_set0(&self);
            }
          }
        } else {
          field = String_charAt_obj(self, i);
          Object_set0(&self);
        }
        Object_free(i);
        e = 0;

      } else if ((self->t == MapObject) || (self->t == ArrayObject) || (self->t == Prototype)) {
        parent = &self->V.m;
        key = Object_toString(p, i);
        Object_free(i);
        if (!key || p->thrw) {
          Object_free(self);
          Object_freeMaybe(key);
          return setThrow(p, "can't convert index to string");
        }
        if ((self->t == ArrayObject) && strncmpEq((Id){.s = key->V.s.s, .len = key->V.s.len}, "length")) {
          field = IntObject_new(List_length(self->V.m));
          Object_set0(&key);
          e = 0;
          Object_set0(&self);
        } else {
          Object *proto = self->t == MapObject ? p->objectPrototype : p->arrayPrototype;
          e = Map_get_str(*parent, key->V.s);
          if (e) {
            Object_set0(&self);
          } else {
            e = Map_get_str(proto->V.m, key->V.s);
            if (e) {
              field = Object_ref(e->value);
              Object_set0(&key);
              e = 0;
            } else {
              Object_set0(&self);
            }
          }
        }

      } else {
        Object_free(self);
        Object_free(i);
        return setThrow(p, "not indexable");
      }
    }
  }
  if (p->err || p->parseErr) {
    return 0;
  }
  i = parseRHS(p, parent, key, e, field, self);
  Object_freeMaybe(key);
  Object_freeMaybe(field);
  Object_freeMaybe(self);
  return i;
}

static Object *
parseTerm(Parser *p);

static Object *
__attribute__((nonnull, warn_unused_result))
parseITerm(Parser *p, Id *id) {
  if (strncmpEq(*id, "new")) {
    skipWs(p);
    if (!parseId(p, id)) {
      return 0;
    }
    if (strncmpEq(*id, "Object")) {
      return MapObject_new();
    } else if (strncmpEq(*id, "Array")) {
      return ArrayObject_new();
    } else if (strncmpEq(*id, "Date")) {
      return DateObject_now(p);
    }
    return setThrow(p, "can't instantiate unknown class");
  } else if (strncmpEq(*id, "typeof")) {
    Object *o = parseTerm(p);
    if (!o) {
      clearErr(p);
      setRunError(p, 0, 0);
      o = &undefinedObject;
    }
    if (!p->nest) {
      Object *t = typeOf(o);
      Object_free(o);
      return t;
    }
    return o;
  } else if (strncmpEq(*id, "undefined")) {
    return &undefinedObject;
  } else if (strncmpEq(*id, "null")) {
    return &nullObject;
  } else if (strncmpEq(*id, "NaN")) {
    return &nanObject;
  } else if (strncmpEq(*id, "function")) {
    if (!expectWs(p, '(')) {
      return 0;
    }
    Object *o = &undefinedObject;
    if (!p->nest) {
      o = FunctionJs_new(p);
    }
    p->nest++;
    if (!parseFunction(p, 0)) {
      Object_free(o);
      return 0;
    }
    p->nest--;
    return o;
  }
  return parseSTerm(p, id);
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseLTerm(Parser *p) {
  char op = 0;
  Object *o;
  if ((o = parseIntLit(p))) {
    return o;
  } else if ((o = parseStringLit(p))) {
    return o;
  } else if ((o = parseObjectLiteral(p))) {
    return o;
  } else if ((o = parseArrayLiteral(p))) {
    return o;
  } else if (acceptWs(p, op = '!') || acceptChar(p, op = '~') || acceptChar(p, op = '-')) {
    if (!(o = parseTerm(p))) {
      return 0;
    }
    if (p->nest) {
      return o;
    }
    int v;
    if (op == '-') {
      if (o->t != IntObject) {
        return setThrow(p, "unary minus expects a number");
      }
      v = -o->V.i;
    } else {
      v = op == '!' ? !toBoolean(o) : o->t == IntObject ? ~o->V.i : ~toBoolean(o);
    }
    Object *b = IntObject_new(v);
    Object_free(o);
    return b;
  } else if (acceptWs(p, '(')) {
    o = parseExpr(p);
    if (!o) {
      return 0;
    }
    if (!expectWs(p, ')')) {
      Object_free(o);
      return 0;
    }
    Object *r = parseRHS(p, 0, 0, 0, o, 0);
    Object_free(o);
    return r;
  }
  p->parseErr = "expected literal, negation, minus or parenthesized expression";
  return 0;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseTerm(Parser *p) {
  Id id;
  if (parseId(p, &id)) {
    return parseITerm(p, &id);
  } else {
    clearErr(p);
    return parseLTerm(p);
  }
}

static char
__attribute__((nonnull, warn_unused_result))
parseOperator(Parser *p) {
  skipWs(p);
  Prog saved = p->prog;
  char op;
  if (acceptChar(p, op = '+')) {
  } else if (acceptChar(p, op = '-')) {
  } else if (acceptChar(p, op = '*')) {
  } else if (acceptChar(p, op = '/')) {
  } else if (acceptChar(p, op = '%')) {
  } else if (acceptChar(p, op = '^')) {
  } else if (acceptChar(p, op = '|')) {
    if (acceptChar(p, '|')) {
      op = 'O';
    }
  } else if (acceptChar(p, op = '&')) {
    if (acceptChar(p, '&')) {
      op = 'A';
    }
  } else if (acceptChar(p, op = '<')) {
    if (acceptChar(p, '<')) {
      op = 'L';
    } else if (acceptChar(p, '=')) {
      op = 'l';
    }
  } else if (acceptChar(p, op = '>')) {
    if (acceptChar(p, '>')) {
      if (acceptChar(p, '>')) {
        op = 'r';
      } else {
        op = 'R';
      }
    } else if (acceptChar(p, '=')) {
      op = 'g';
    }
  } else if (acceptChar(p, op = '=')) {
    if (!expectChar(p, '=') || !expectChar(p, '=')) {
      p->prog = saved;
      return 0;
    }
  } else if (acceptChar(p, op = '!')) {
    if (!expectChar(p, '=') || !expectChar(p, '=')) {
      p->prog = saved;
      return 0;
    }
  } else {
    op = 0;
  }
  return op;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseOperatorTerm(Parser *p, Object *t1, char op) {
  int sh = 0;
  int isBool = 1;
  switch (op) {
    case 'A':
      sh = !toBoolean(t1);
      break;
    case 'O':
      sh = toBoolean(t1);
      break;
    default:
      isBool = 0;
  }
  if (sh) {
    p->nest++;
  }

  Object *t2 = parseTerm(p);
  if (!t2) {
    Object_free(t1);
    return 0;
  }

  if (p->nest) {
    if (sh) {
      p->nest--;
    }
    Object_free(t2);
    return t1;
  }

  if (isBool) {
    Object_free(t1);
    switch (op) {
      case 'A':
      case 'O':
        return t2;
      default:
        /* coverage:unreachable */
        Object_free(t2);
        return 0;
        /* /coverage:unreachable */
    }

  } else if ((op == '=') || (op == '!')) {
    int b = isString(t1) && isString(t2) ? isStringEq(t1->V.s, t2->V.s) :
      t1->t == t2->t ?
        t1->t == IntObject ? t1->V.i == t2->V.i :
        (t1->t == MapObject) || (t1->t == ArrayObject) || (t1->t == Prototype) || (t1->t == DateObject) ? t1 == t2 :
        t1->t == FunctionJs ? t1->V.j.p.s == t2->V.j.p.s :
        t1->t == FunctionNative ? t1->V.f == t2->V.f :
        /* coverage:no */
        t1->t == MethodNative ? t1->V.a == t2->V.a :
        /* /coverage:no */
        (t1->t == UndefinedObject) || (t1->t == NullObject)
        :
      0;

    Object_free(t1);
    Object_free(t2);
    return IntObject_new((op == '=') ? b : !b);

  } else if ((t1->t == IntObject) && (t1->t == t2->t)) {
    int x = t1->V.i;
    int y = t2->V.i;
    Object_set0(&t1);
    Object_set0(&t2);
    switch (op) {
      case '+':
        return IntObject_new(x + y);
      case '-':
        return IntObject_new(x - y);
      case '*':
        return IntObject_new(x * y);
      case '/':
        return IntObject_new(x / y);
      case '%':
        return IntObject_new(x % y);
      case '^':
        return IntObject_new(x ^ y);
      case '|':
        return IntObject_new(x | y);
      case '&':
        return IntObject_new(x & y);
      case 'R':
        return IntObject_new(x >> y);
      case 'r':
        return IntObject_new((int)((unsigned int)x >> y));
      case 'L':
        return IntObject_new(x << y);
      case '<':
        return IntObject_new(x < y);
      case 'l':
        return IntObject_new(x <= y);
      case '>':
        return IntObject_new(x > y);
      case 'g':
        return IntObject_new(x >= y);
      default: {}
    }

  } else if ((t1->t == DateObject) && (t2->t == IntObject)) {
    long d = t1->V.d;
    int y = t2->V.i;
    Object_set0(&t1);
    Object_set0(&t2);
    switch (op) {
      case '-':
        return DateObject_new(d - y);
      case '/':
        d /= y;
        if ((d >= INT_MIN) && (d <= INT_MAX)) {
          return IntObject_new((int)d);
        }
        return setThrow(p, "integer overflow");
      case '%':
        return IntObject_new((int)(d % y));
      default: {}
    }

  } else if ((t1->t == DateObject) && (t1->t == t2->t) && (op == '-')) {
    long d = t1->V.d - t2->V.d;
    Object_free(t1);
    Object_free(t2);
    if ((d >= INT_MIN) && (d <= INT_MAX)) {
      return IntObject_new((int)d);
    }
    return setThrow(p, "integer overflow");

  } else if (op == '+') {
    Object *s = 0;
    if (isString(t1)) {
      s = t2;
      t2 = Object_toString(p, s);
      Object_free(s);
    } else if (isString(t2)) {
      s = t1;
      t1 = Object_toString(p, s);
      Object_free(s);
    }
    if (s && t1 && t2 && !p->thrw) {
      Object *o = String_concat(t1, t2);
      Object_free(t1);
      Object_free(t2);
      return o;
    }
  }

  Object_freeMaybe(t1);
  Object_freeMaybe(t2);
  if (p->thrw) {
    return &undefinedObject;
  }
  return setThrow(p, "unknown operand types for expression");
}

static Object *
__attribute__((nonnull(1), warn_unused_result))
parseEExpr(Parser *p, Object *t1) {
  if (!t1) {
    return 0;
  }
  clearErr(p);
  char op = parseOperator(p);
  char opFirst = op;
  while (op) {
    if (op != opFirst) {
      Object_free(t1);
      p->parseErr = "Please use parenthesis for differing operators";
      return 0;
    }
    if (!(t1 = parseOperatorTerm(p, t1, op))) {
      return 0;
    }
    op = parseOperator(p);
  }
  clearErr(p);
  return t1;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseIExpr(Parser *p, Id *id) {
  return parseEExpr(p, parseITerm(p, id));
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseLExpr(Parser *p) {
  return parseEExpr(p, parseLTerm(p));
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseExpr(Parser *p) {
  Id id;
  if (parseId(p, &id)) {
    return parseIExpr(p, &id);
  } else {
    clearErr(p);
    return parseLExpr(p);
  }
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseWhile(Parser *p) {
  if (!expectWs(p, '(')) {
    return 0;
  }

  const char *begin = p->prog.s;
  Object *o;
  int cond = 1;
  do {
    p->prog.s = begin;
    if (!(o = parseExpr(p)) || !expectChar(p, ')')) {
      return 0;
    }
    if (!p->nest) {
      if (!toBoolean(o)) {
        p->nest++;
        cond = 0;
      }
    }
    Object_free(o);
    if (!(o = parseBlock(p))) {
      return 0;
    }
    Object_free(o);
    if (!cond) {
      p->nest--;
    }
  } while (!p->nest && cond);
  return &undefinedObject;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseFor(Parser *p) {
  if (!expectWs(p, '(')) {
    return 0;
  }

  Id itName, word;
  Object *e;
  if (!parseId(p, &itName) || !parseId(p, &word) || !strncmpEq(word, "in") || !(e = parseExpr(p))) {
    return 0;
  }
  if (!p->nest && ((e->t != MapObject) && (e->t != Prototype))) {
    Object_free(e);
    return setRunError(p, "for can only iterate on Object", &itName);
  }
  if (!expectWs(p, ')')) {
    return 0;
  }

  List *l = e->V.m;
  int isEmpty = !l;
  if (isEmpty) {
    p->nest++;
  }

  skipWs(p);
  const char *begin = p->prog.s;
  do {
    p->prog.s = begin;
    Object *caller = 0;
    Object *o;
    if (!p->nest) {
      caller = p->vars;
      p->vars = MapObject_new();
      Map_set_const(&p->vars->V.m, "", caller);
      Map_set_id(&p->vars->V.m, &itName, o = StringObject_new(l->key));
      Object_free(o);
    }

    o = parseBody(p);
    if (caller) {
      Object_setUnref(&p->vars, caller);
      l = l->next;
    }
    if (!o) {
      Object_free(e);
      return 0;
    }
    Object_free(o);
  } while (!p->nest && l);

  if (isEmpty) {
    p->nest--;
  }
  Object_free(e);
  return &undefinedObject;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseIf(Parser *p) {
  Object *o;
  if (!expectWs(p, '(') || !(o = parseExpr(p))) {
    return 0;
  }
  int cond = !p->nest && toBoolean(o);
  if (!cond) {
    p->nest++;
  }
  Object_free(o);
  if (!expectWs(p, ')') || !(o = parseBlock(p))) {
    return 0;
  }
  Object_free(o);
  if (!cond) {
    p->nest--;
  }

  const char *saved = p->prog.s;
  Id id;
  if (!parseId(p, &id)) {
    clearErr(p);
    return &undefinedObject;
  }

  if (!strncmpEq(id, "else")) {
    p->prog.s = saved;
    return &undefinedObject;
  }
  if (cond) {
    p->nest++;
  }
  if (!(o = parseBlock(p))) {
    return 0;
  }
  Object_free(o);
  if (cond) {
    p->nest--;
  }
  return &undefinedObject;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseStatement(Parser *p) {
  Object *o = 0;
  Id id;
  p->needSemicolon = 1;

  if (parseId(p, &id)) {
    if (strncmpEq(id, "if")) {
      o = parseIf(p);
    } else if (strncmpEq(id, "while")) {
      o = parseWhile(p);
    } else if (strncmpEq(id, "for")) {
      o = parseFor(p);
    } else if (strncmpEq(id, "try")) {
      if (!(o = parseBody(p))) {
        return 0;
      }
      Object_free(o);
      if (!parseId(p, &id) || !strncmpEq(id, "catch") || !expectWs(p, '(') || !parseId(p, &id) || !expectWs(p, ')')) {
        return 0;
      }
      if (p->nest && p->thrw) {
        p->nest--;

        Object *caller = 0;
        if (!p->nest) {
          caller = p->vars;
          p->vars = MapObject_new();
          Map_set_const(&p->vars->V.m, "", caller);
          Map_set_id(&p->vars->V.m, &id, p->thrw);
        }
        Object_set0(&p->thrw);
        if ((o = parseBody(p))) {
          Object_set(&o, &undefinedObject);
        }
        if (caller) {
          Object_setUnref(&p->vars, caller);
        }
      } else {
        p->nest++;
        if ((o = parseBody(p))) {
          Object_set(&o, &undefinedObject);
        }
        p->nest--;
      }
    } else if (strncmpEq(id, "var")) {
      if (parseId(p, &id)) {
        if (acceptWs(p, '=')) {
          o = parseExpr(p);
        } else {
          o = &undefinedObject;
        }
        if (o) {
          if (!p->nest) {
            Map_set_id(&p->vars->V.m, &id, o);
          }
          Object_set(&o, &undefinedObject);
        }
      }
    } else if (strncmpEq(id, "function")) {
      if (parseId(p, &id) && expectWs(p, '(')) {
        if (!p->nest) {
          Object *f = FunctionJs_new(p);
          Map_set_id(&p->vars->V.m, &id, f);
          Object_free(f);
        }
        p->nest++;
        if ((o = parseFunction(p, 0))) {
          p->nest--;
        }
      }
    } else if (strncmpEq(id, "return")) {
      if ((o = parseExpr(p)) && !p->nest) {
        p->ret = o;
        o = 0;
      }
    } else if (strncmpEq(id, "throw")) {
      if ((o = parseExpr(p)) && !p->nest) {
        p->thrw = o;
        p->nest++;
        o = &undefinedObject;
      }
    } else {
      o = parseIExpr(p, &id);
    }
  } else {
    clearErr(p);
    o = parseLExpr(p);
  }
  return o;
}

static int
__attribute__((pure, nonnull, warn_unused_result))
haveMore(Parser *p) {
  return !p->parseErr && (p->prog.s < p->prog.end);
}

static int
__attribute__((nonnull, warn_unused_result))
parseRequiredSemicolon(Parser *p) {
  if (p->needSemicolon) {
    return 0;
  }
  if (acceptWs(p, ';')) {}
  return 1;
}

static void
__attribute__((nonnull))
parseOptionalSemicolon(Parser *p) {
  if (acceptWs(p, ';')) {
    p->needSemicolon = 0;
  }
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseStatements(Parser *p) {
  p->needSemicolon = 0;
  Object *o = &undefinedObject;
  skipWs(p);
  while (o && haveMore(p)) {
    Object_free(o);
    if (!parseRequiredSemicolon(p)) {
      return 0;
    }
    o = parseStatement(p);
    if (o) {
      parseOptionalSemicolon(p);
      skipWs(p);
    }
  }
  return o;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseBodyInner(Parser *p) {
  p->needSemicolon = 0;
  Object *o;
  while (!acceptWs(p, '}')) {
    if (!parseRequiredSemicolon(p) || !(o = parseStatement(p))) {
      return 0;
    }
    Object_free(o);
    parseOptionalSemicolon(p);
  }
  p->needSemicolon = 0;
  return &undefinedObject;
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseBody(Parser *p) {
  if (!expectWs(p, '{')) {
    return 0;
  }
  return parseBodyInner(p);
}

static Object *
__attribute__((nonnull, warn_unused_result))
parseBlock(Parser *p) {
  if (acceptWs(p, '{')) {
    return parseBodyInner(p);
  }
  Object *o = parseStatement(p);
  parseOptionalSemicolon(p);
  return o;
}

/* coverage:stdout */
static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1)))
Console_log(Parser *p, List *l) {
  Object *e = l ? l->value : &undefinedObject;
  Object *os = Object_toString(p, e);
  if (os && !p->thrw) {
    putsn(os->V.s.s, os->V.s.len, stdout);
    Object_free(os);
  } else {
    /* coverage:no */
    Object_freeMaybe(os);
    fputs("?", stdout);
    /* /coverage:no */
  }
  return &undefinedObject;
}
/* /coverage:stdout */

static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1)))
String_fromCharCode(Parser *p, List *l) {
  Object *e = l ? l->value : &undefinedObject;
  char c = 0;
  if ((e->t == IntObject) && (e->V.i >= 0) && (e->V.i < 128)) {
    c = (char)e->V.i;
  }
  return StringObject_new_char(c);
}

/* coverage:stdin */
static Object *
__attribute__((nonnull(1), warn_unused_result))
process_stdin_on(Parser *p, List *l) {
  if (!l || !l->next || !isString(l->value) || !strncmpEq(l->value->V.c, "data")) {
    return setThrow(p, "expecting String and Function argument");
  }
  Object_setUnref(&p->onStdinData, l->next->value->t == FunctionJs ? Object_ref(l->next->value) : 0);
  return &undefinedObject;
}
/* /coverage:stdin */

static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1)))
global_isNaN(Parser *p, List *l) {
  return IntObject_new(l ? l->value->t == NanObject : 0);
}

/* coverage:file */
static Object *
__attribute__((warn_unused_result, nonnull(1)))
fs_readFile(Parser *p, List *l) {
  if (!l || !isString(l->value) || !l->next || (l->next->value->t != FunctionJs)) {
    return setThrow(p, "expecting String and Function argument");
  }
  char *name = strndup(l->value->V.c.s, l->value->V.c.len);
  Object *err;
  Object *data = &undefinedObject;

  const int fd = open(name, O_RDONLY);
  free(name);
  if (fd < 0) {
    err = StringObject_new("readFile open failed");
  } else {
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
      /* coverage:no */
      close(fd);
      err = StringObject_new("readFile stat failed");
      /* /coverage:no */
    } else {
      size_t len = off_t2size_t(sb.st_size);
      char *s = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
      if (s == (void*)-1) {
        /* coverage:no */
        close(fd);
        err = StringObject_new("readFile mmap failed");
        /* /coverage:no */
      } else {
        err = &undefinedObject;
        data = Mmap_new(s, len, fd);
      }
    }
  }

  List *args = List_new(List_new(0, 0, data), 0, err);
  Object_freeMaybe(invokeFun(p, l->next->value, args, 0));
  List_free(args);
  return &undefinedObject;
}

static Object *
__attribute__((warn_unused_result, nonnull(1)))
fs_writeFile(Parser *p, List *l) {
  if (!l || !isString(l->value) || !l->next || !isString(l->next->value) || !l->next->next || (l->next->next->value->t != FunctionJs)) {
    return setThrow(p, "expecting String, String and Function argument");
  }
  char *name = strndup(l->value->V.c.s, l->value->V.c.len);
  Object *err;

  const int fd = open(name, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
  free(name);
  if (fd < 0) {
    err = StringObject_new("writeFile open failed");
  } else {
    ssize_t wn = write(fd, l->next->value->V.s.s, l->next->value->V.s.len);
    if ((wn < 0) || ((size_t)wn < l->next->value->V.s.len)) {
      /* coverage:no */
      err = StringObject_new("write failed to save enough data");
      /* /coverage:no */
    } else {
      err = &undefinedObject;
    }
    close(fd);
  }

  List *args = List_new(0, 0, err);
  Object_freeMaybe(invokeFun(p, l->next->next->value, args, 0));
  List_free(args);
  return &undefinedObject;
}

static Object *
__attribute__((warn_unused_result, nonnull(1)))
fs_readDir(Parser *p, List *l) {
  if (!l || !isString(l->value) || !l->next || (l->next->value->t != FunctionJs)) {
    return setThrow(p, "expecting String and Function argument");
  }
  Object *err = &undefinedObject;
  Object *o = ArrayObject_new();
  char *name = strndup(l->value->V.s.s, l->value->V.s.len);
  DIR *dirp = opendir(name);
  free(name);

  if (!dirp) {
    err = StringObject_new("opendir failed");
  } else {
    Object *i = IntObject_new(0);
    List **tail = &o->V.m;
    struct dirent *ent;
    while ((errno = 0, ent = readdir(dirp))) {
      const char *s = ent->d_name;
      if ((s[0] == '.') && (!s[1] || ((s[1] == '.') && !s[2]))) {
        continue;
      }
      List_push(p, &tail, i, StringObject_new_dup(ent->d_name));
    }
    Object_free(i);

    if (errno) {
      /* coverage:no */
      err = StringObject_new("readdir failed");
      /* /coverage:no */
    }
    /* coverage:no */
    if (closedir(dirp) && (err == &undefinedObject)) {
      err = StringObject_new("closedir failed");
    }
    /* /coverage:no */
  }

  List *args = List_new(List_new(0, 0, o), 0, err);
  Object_freeMaybe(invokeFun(p, l->next->value, args, 0));
  List_free(args);
  return &undefinedObject;
}
/* /coverage:file */

static void
__attribute__((nonnull))
addField(Object *o, const char *key, Object *v) {
  Map_set_const(&o->V.m, key, v);
  Object_free(v);
}

static void
__attribute__((nonnull))
addFunction(Object *o, const char *key, Native f) {
  addField(o, key, FunctionNative_new(f));
}

static void
__attribute__((nonnull))
addMethod(Object *o, const char *key, MethodFun a) {
  addField(o, key, MethodNative_new(a));
}

static Object *
global_eval(Parser *p, List *l);

static Object *
global_eval2(Parser *p, List *l);

static Object *
__attribute__((nonnull, returns_nonnull, warn_unused_result))
addPrototype(Object *parent) {
  Object *o = Prototype_new();
  addField(parent, "prototype", o);
  return o;
}

/* coverage:net */
static Object *
__attribute__((warn_unused_result, nonnull(1)))
node_net_connection_end(Parser *p, List *l) {
  Object_set0(&p->connClient);
  Object_set0(&p->connOptions);
  Object_set0(&p->onConnData);
  Object_set0(&p->onConnEnd);
  Object_set0(&p->onConnError);

  if (p->sock != -1) {
    if (shutdown(p->sock, SHUT_RDWR)) {
      /* coverage:no */
      close(p->sock);
      p->sock = -1;
      /* /coverage:no */
      return &undefinedObject;
    }
    if (close(p->sock)) {
      /* coverage:no */
      p->sock = -1;
      return &undefinedObject;
      /* /coverage:no */
    }
    p->sock = -1;
  }
  return &undefinedObject;
}

static void
__attribute__((nonnull))
node_net_errorConnecting(Parser *p, const char *err) {
  Object *onConnError = p->onConnError ? Object_ref(p->onConnError) : 0;
  if (node_net_connection_end(p, 0)) {};
  if (onConnError) {
    List *args = List_new(0, 0, StringObject_new(err));
    Object_freeMaybe(invokeFun(p, onConnError, args, 0));
    List_free(args);
    Object_free(onConnError);
  }
}

static void
__attribute__((nonnull))
node_net_startConnect(Parser *p, Object *onConnect) {
  List *o = p->connOptions->V.m;
  List *e;
  if (!(e = Map_get_const(o, "port")) || (e->value->t != IntObject) || (e->value->V.i <= 0) || (e->value->V.i > 65535)) {
    /* coverage:netsmoke */
    node_net_errorConnecting(p, "expecting port");
    return;
    /* /coverage:netsmoke */
  }
  Object *port = Object_toString(p, e->value);
  if (!(e = Map_get_const(o, "host")) || !isString(e->value)) {
    /* coverage:netsmoke */
    Object_free(port);
    node_net_errorConnecting(p, "expecting host");
    return;
    /* /coverage:netsmoke */
  }
  char *host = strndup(e->value->V.c.s, e->value->V.c.len);

  struct addrinfo *addrs;
  struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_flags = AI_ADDRCONFIG};
  int err = getaddrinfo(host, port->V.c.s, &hints, &addrs);
  Object_free(port);
  free(host);
  if (err) {
    node_net_errorConnecting(p, "failed to get address of host");
    return;
  }
  p->sock = 0;
  struct addrinfo *addr = addrs;
  for (; addr; addr = addr->ai_next) {
    p->sock = socket(addr->ai_family, addr->ai_socktype | SOCK_CLOEXEC, addr->ai_protocol);
    if (p->sock < 0) {
      /* coverage:no */
      continue;
      /* /coverage:no */
    }
    if (!connect(p->sock, addr->ai_addr, addr->ai_addrlen)) {
      break;
    }
    close(p->sock);
  }
  freeaddrinfo(addrs);
  if (!addr) {
    node_net_errorConnecting(p, "failed to connect to any address");
    return;
  }

  List *args = List_new(0, 0, Object_ref(p->connClient));
  Prog saved = p->prog;
  Object *cb = invokeFun(p, onConnect, args, 0);
  List_free(args);
  if (!cb) {
    clearErr(p);
    setRunError(p, 0, 0);
    p->prog = saved;
    node_net_errorConnecting(p, "failed onconnect event handler");
    return;
  }
  Object_free(cb);
  if (p->thrw) {
    Object_set0(&p->thrw);
    p->nest--;
    node_net_errorConnecting(p, "exception in onconnect event handler");
    return;
  }
}

static Object *
__attribute__((warn_unused_result, nonnull(1)))
node_net_connection_on(Parser *p, List *l) {
  if (!l || !isString(l->value) || !l->next || (l->next->value->t != FunctionJs)) {
    /* coverage:netsmoke */
    return setThrow(p, "expecting String and Function argument");
    /* /coverage:netsmoke */
  }
  if (strncmpEq(l->value->V.c, "data")) {
    Object_set(&p->onConnData, l->next->value);
  } else if (strncmpEq(l->value->V.c, "connect")) {
    node_net_startConnect(p, l->next->value);
  } else if (strncmpEq(l->value->V.c, "end")) {
    Object_set(&p->onConnEnd, l->next->value);
  } else if (strncmpEq(l->value->V.c, "error")) {
    Object_set(&p->onConnError, l->next->value);
  } else {
    /* coverage:netsmoke */
    return setThrow(p, "expecting 'connect', 'data', 'end' or 'error' as argument");
    /* /coverage:netsmoke */
  }
  return &undefinedObject;
}

static Object *
__attribute__((warn_unused_result, nonnull(1)))
node_net_connection_write(Parser *p, List *l) {
  if (!l || !isString(l->value)) {
    return setThrow(p, "expecting String argument");
  }
  if (p->sock == -1) {
    /* coverage:unreachable */
    return setThrow(p, "socket closed");
    /* /coverage:unreachable */
  }
  ssize_t wn = write(p->sock, l->value->V.c.s, l->value->V.c.len);
  if ((wn < 0) || ((size_t)wn < l->value->V.c.len)) {
    /* coverage:no */
    if (p->onConnEnd) {
      Object_freeMaybe(invokeFun(p, p->onConnEnd, 0, 0));
    }
    if (node_net_connection_end(p, 0)) {}
    /* /coverage:no */
  }
  return &undefinedObject;
}

static Object *
__attribute__((warn_unused_result, nonnull(1)))
node_net_createConnection(Parser *p, List *l) {
  if (!l || (l->value->t != MapObject)) {
    /* coverage:netsmoke */
    return setThrow(p, "expecting Object argument");
    /* /coverage:netsmoke */
  }
  p->connOptions = Object_ref(l->value);

  Object *ret = MapObject_new();
  addFunction(ret, "on", node_net_connection_on);
  addFunction(ret, "end", node_net_connection_end);
  addFunction(ret, "write", node_net_connection_write);
  p->connClient = Object_ref(ret);
  return ret;
}
/* /coverage:net */

/* coverage:smokesystem */
static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1)))
global_require(Parser *p, List *l) {
  if (!l || !isString(l->value)) {
    return setThrow(p, "expecting String argument");
  }
  if (strncmpEq(l->value->V.c, "fs")) {
    Object *o = MapObject_new();
    addFunction(o, "readFile", &fs_readFile);
    addFunction(o, "writeFile", &fs_writeFile);
    addFunction(o, "readdir", &fs_readDir);
    return o;
  } else if (strncmpEq(l->value->V.c, "node:net")) {
    /* coverage:net */
    Object *o = MapObject_new();
    addFunction(o, "createConnection", &node_net_createConnection);
    return o;
    /* /coverage:net */
  } else {
    return &undefinedObject;
  }
}

static Object *
__attribute__((returns_nonnull, warn_unused_result, nonnull(1)))
global_setTimeout(Parser *p, List *l) {
  if (!l || (l->value->t != FunctionJs) || (!l->next) || (l->next->value->t != IntObject)) {
    return setThrow(p, "expecting Function and Int argument");
  }
  Object_set(&p->onTimeout, l->value);
  p->timeoutMs = l->next->value->V.i;
  return &undefinedObject;
}
/* /coverage:smokesystem */

Parser *
Parser_new(void) {
  Parser *p = malloc(sizeof(*p));
  p->vars = MapObject_new();

  Object *con = MapObject_new();
  addFunction(con, "log", &Console_log);
  addField(p->vars, "console", con);

  Object *s = MapObject_new();
  addFunction(s, "fromCharCode", &String_fromCharCode);
  p->stringPrototype = addPrototype(s);
  addMethod(p->stringPrototype, "indexOf", &String_indexOf);
  addMethod(p->stringPrototype, "charCodeAt", &String_charCodeAt);
  addMethod(p->stringPrototype, "charAt", &String_charAt);
  addMethod(p->stringPrototype, "match", &String_match);
  addField(p->vars, "String", s);

  Object *o = MapObject_new();
  p->objectPrototype = addPrototype(o);
  addField(p->vars, "Object", o);

  Object *a = MapObject_new();
  p->arrayPrototype = addPrototype(a);
  addField(p->vars, "Array", a);

  Object *ps = MapObject_new();
  addFunction(ps, "on", &process_stdin_on);
  Object *pr = MapObject_new();
  addField(pr, "stdin", ps);
  addField(p->vars, "process", pr);

  addFunction(p->vars, "isNaN", &global_isNaN);
  addFunction(p->vars, "eval", &global_eval);
  addFunction(p->vars, "eval2", &global_eval2);
  addFunction(p->vars, "require", &global_require);
  addFunction(p->vars, "setTimeout", &global_setTimeout);

  p->onTimeout = 0;
  p->onStdinData = 0;
  p->connClient = 0;
  p->connOptions = 0;
  p->onConnData = 0;
  p->onConnEnd = 0;
  p->onConnError = 0;
  p->sock = -1;
  return p;
}

static Parser *
__attribute__((malloc, returns_nonnull, warn_unused_result, nonnull))
Parser_new_vars(Object *vars) {
  Parser *p = Parser_new();
  addField(vars, "", p->vars);
  p->vars = Object_ref(vars);
  return p;
}

static void
__attribute__((nonnull))
showParseError(Parser *p) {
  if (p->debug) {
    /* coverage:stderr */
    fputs("parse error: ", stderr);
    if (p->parseErr) {
      fputs(p->parseErr, stderr);
      if (p->parseErrChar) {
        fputs(" ( '", stderr);
        fputc(p->parseErrChar, stderr);
        fputs("' )", stderr);
      }
    }
    fputs("\n", stderr);
    showProg(p);
    /* /coverage:stderr */
  }
}

static void
__attribute__((nonnull))
showRunError(Parser *p) {
  if (p->debug) {
    /* coverage:stderr */
    fputs("runtime error: ", stderr);
    if (p->err) {
      fputs(p->err, stderr);
      if (p->errName.s) {
        fputs(" - ", stderr);
        putsn(p->errName.s, p->errName.len, stderr);
      }
    }
    fputs("\n", stderr);
    showProg(p);
    /* /coverage:stderr */
  }
}

static void
__attribute__((nonnull))
Parser_evalInit(Parser *p) {
  p->err = 0;
  p->nest = 0;
  p->ret = 0;
  p->thrw = 0;
  p->needSemicolon = 0;
  clearErr(p);
}

static Object *
__attribute__((nonnull, warn_unused_result))
Parser_evalString(Parser *p, Object *prog) {
  Parser_evalInit(p);
  p->prog = (Prog){.s = prog->V.c.s, .end = prog->V.c.s + prog->V.c.len, .h = prog};
  return parseStatements(p);
}

static Object *
__attribute__((nonnull, warn_unused_result))
Parser_evalWithThrow(Parser *p, Object *prog) {
  Object *o;
  if (isString(prog)) {
    o = Parser_evalString(p, prog);
  } else if (prog->t == FunctionJs) {
    Parser_evalInit(p);
    p->prog = (Prog){.s = 0, .end = 0, .h = prog->V.j.scope};
    o = invokeFun(p, prog, 0, 0);
  } else {
    return 0;
  }
  Object *thrw = 0;
  if (p->ret) {
    Object_set0(&p->ret);
    if (!p->thrw) {
      thrw = StringObject_new("eval: return outside function");
    }
  } else if (!o && !p->thrw) {
    if (p->parseErr || !p->err) {
      thrw = StringObject_new("eval: parse error");
    } else {
      thrw = StringObject_new("eval: runtime error");
    }
  }
  clearErr(p);
  setRunError(p, 0, 0);
  if (thrw) {
    p->thrw = thrw;
    p->nest++;
    Object_set(&o, &undefinedObject);
  }
  return o;
}

static Object *
__attribute__((nonnull(1), warn_unused_result))
global_eval(Parser *p, List *l) {
  if (!l || !isString(l->value)) {
    return setThrow(p, "expecting String argument");
  }
  Prog prog = p->prog;
  Object *o = Parser_evalWithThrow(p, l->value);
  p->prog = prog;
  return o;
}

static int
__attribute__((const, warn_unused_result))
timeout2int(long int timeOut) {
  return timeOut < 0 ? 0 : timeOut > 60000 ? 60000 : (int)timeOut;
}

static Object *
__attribute__((nonnull(1), warn_unused_result))
global_eval2(Parser *p, List *l) {
  if (!l || (l->value->t != MapObject) || !l->next || (!isString(l->next->value) && (l->next->value->t != FunctionJs))) {
    return setThrow(p, "expecting an Object and a String or Function argument");
  }

  Parser *q = Parser_new_vars(l->value);
  List *r = Map_get_const(l->value->V.m, ".onTimeout");
  /* coverage:smoke */
  q->onTimeout = r && (r->value->t != UndefinedObject) ? Object_ref(r->value) : 0;
  /* /coverage:smoke */
  r = Map_get_const(l->value->V.m, ".timeoutMs");
  q->timeoutMs = r && (r->value->t == IntObject) ? r->value->V.i : 0;
  q->debug = p->debug;

  Object *o = Parser_evalWithThrow(q, l->next->value);

  if (q->thrw) {
    p->thrw = q->thrw;
    p->nest++;
  }
  if (!o) {
    /* coverage:unreachable */
    o = &undefinedObject;
    /* /coverage:unreachable */
  }
  /* coverage:smoke */
  Map_set_const(&l->value->V.m, ".onTimeout", q->onTimeout ? q->onTimeout : &undefinedObject);
  /* /coverage:smoke */
  Object *t = IntObject_new(timeout2int(q->timeoutMs));
  Map_set_const(&l->value->V.m, ".timeoutMs", t);
  Object_free(t);
  Parser_free(q);
  return o;
}

static int
__attribute__((warn_unused_result))
Parser_evalResult(Parser *p, Object *o) {
  if (o) {
    int ret;
    if (p->thrw) {
      if (p->debug) {
        /* coverage:stderr */
        Object *thrown = p->thrw;
        p->thrw = 0;
        Object *os = Object_toString(p, thrown);
        Object_free(thrown);
        fputs("runtime error: exception\n", stderr);
        if (os && !p->thrw) {
          putsn(os->V.s.s, os->V.s.len, stderr);
        }
        Object_freeMaybe(os);
        fputs("\n", stderr);
        /* /coverage:stderr */
      } else {
        Object_set0(&p->thrw);
      }
      ret = -2;
    } else {
      ret = o->t != IntObject ? toBoolean(o) : o->V.i > 0 ? o->V.i : 0;
    }
    Object_free(o);
    return ret;
  }

  if (p->ret) {
    Object_set0(&p->ret);
    if (p->debug) {
      /* coverage:stderr */
      fputs("runtime error: return outside function", stderr);
      fputs("\n", stderr);
      /* /coverage:stderr */
    }
    return -2;
  }

  Object_set0(&p->thrw);
  if (p->parseErr || !p->err) {
    showParseError(p);
    return -1;
  } else {
    showRunError(p);
    return -2;
  }
}

int
Parser_eval(Parser *p, const char *prog, size_t len, int debug) {
  p->debug = debug;
  Object *s = StringObject_new_const((Id){.s = prog, .len = len, .h = 0});
  Object *o = Parser_evalString(p, s);
  addField(p->vars, "", s);
  return Parser_evalResult(p, o);
}

/* coverage:smokesystem */
int
Parser_eventLoop(Parser *p, const char *prog, size_t plen, int debug) {
  while (p->onTimeout || p->onStdinData || p->onConnData) {
    fd_set rfd;
    FD_ZERO(&rfd);
    int fds = 0;
    if (p->onStdinData) {
      /* coverage:stdin */
      FD_SET(STDIN_FILENO, &rfd);
      fds++;
      /* /coverage:stdin */
    }
    if (p->sock != -1) {
      /* coverage:no */
      FD_SET(p->sock, &rfd);
      /* /coverage:no */
      /* coverage:net */
      fds = p->sock + 1;
      /* /coverage:net */
    }

    int ret;
    if (p->onTimeout) {
      struct timeval timeout;
      timeout.tv_sec = p->timeoutMs / 1000;
      timeout.tv_usec = (p->timeoutMs % 1000) * 1000;
      ret = select(fds, &rfd, NULL, NULL, &timeout);
    } else {
      /* coverage:stdin */
      ret = select(fds, &rfd, NULL, NULL, NULL);
      /* /coverage:stdin */
    }

    if (ret < 0) {
      /* coverage:no */
      perror("select");
      return -2;
      /* /coverage:no */
    } else if (ret == 0) {
      if (p->onTimeout) {
        Object *onTimeout = p->onTimeout;
        p->onTimeout = 0;
        Object *o = invokeFun(p, onTimeout, 0, 0);
        Object_free(onTimeout);
        if (Parser_evalResult(p, o)) {}
      }
    }

    /* coverage:stdin */
    if (p->onStdinData && FD_ISSET(STDIN_FILENO, &rfd)) {
      size_t all = 0;
      Str s = {.s = 0};
      ssize_t len = getline(&s.s, &all, stdin);
      Object *arg;
      if (len <= 0) {
        mfree(s.s);
        arg = &undefinedObject;
      } else {
        s.len = (size_t)len - (s.s[len - 1] == '\n' ? 1 : 0);
        arg = StringObject_new_str(s);
      }
      List *args = List_new(0, 0, arg);
      Object *o = invokeFun(p, p->onStdinData, args, 0);
      List_free(args);
      if (Parser_evalResult(p, o)) {}
    }
    /* /coverage:stdin */
    /* coverage:no */
    if ((p->sock != -1) && p->onConnData && FD_ISSET(p->sock, &rfd)) {
    /* /coverage:no */
    /* coverage:net */
      char buf[2048];
      ssize_t nread = read(p->sock, &buf, sizeof(buf));
      Object *o = &undefinedObject;
      if (nread <= 0) {
        if (p->onConnEnd) {
          o = invokeFun(p, p->onConnEnd, 0, 0);
        }
        if (node_net_connection_end(p, 0)) {}
      } else {
        Object *arg = StringObject_new_const((Id){.s = buf, .len = (size_t)nread});
        List *args = List_new(0, 0, arg);
        o = invokeFun(p, p->onConnData, args, 0);
        List_free(args);
      }
      if (Parser_evalResult(p, o)) {}
    }
    /* /coverage:net */
  }
  return Parser_eval(p, prog, plen, debug);
}
/* /coverage:smokesystem */
