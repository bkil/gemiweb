#include <stdio.h> // puts fputs fputc putchar stdout
#include <malloc.h> // malloc free
#include <stdlib.h> // atoi size_t
#include <string.h> // strcmp strncmp strncpy strdup strndup strlen

#ifdef DEBUG
#include "vm-debug.h"
#endif

#include "vm2.h"


#ifdef SMALLBIN
#include "vm2-smallbin.c"
#define NEEDLEAK

#else

#include <stdio.h> // snprintf

static int
snprinti(char *s, size_t n, int i) {
  return snprintf(s, n, "%d", i);
}

#ifdef SIMPLE
#define NEEDLEAK
#endif
#endif

#ifdef NEEDLEAK

#define getCycles(o) (0)
#define Object_ref(o) (o)
#define Object_clone(o) (o)
#define mfree(o) while(0) {}
#define Object_free(o) while(0) {}
#define List_free(l) while(0) {}

void __attribute__((const))
Parser_free(Parser *p) {
}

#else
Object *MapObject_new(void);
List *List_new(List *next, char *key, Object *value);
#define mfree(o) free(o)

#include "vm2-free.c"
#endif

static Object *
setRunError(Parser *p, const char *message, const Id *id) {
  p->err = message;
  p->errName = id ? *id : (Id){0};
  return 0;
}

#ifdef NEEDLEAK
static
#endif
List *
List_new(List *next, char *key, Object *value) {
  List *l = malloc(sizeof(*l));
  l->next = next;
  l->key = key;
  l->value = value;
  return l;
}

static Object undefinedObject = {.ref = -1, .t = UndefinedObject, .i = 0};
static Object nullObject = {.ref = -1, .t = NullObject, .i = 0};

static Object *
IntObject_new(int x) {
  Object *o = malloc(sizeof(*o));
  o->ref = 0;
  o->t = IntObject;
  o->i = x;
  return o;
}

static Object *
StringObject_new_str(Str s) {
  Object *o = malloc(sizeof(*o));
  o->ref = 0;
  o->t = StringObject;
  o->s = (Str){.s = s.s, .len = s.len};
  return o;
}

static Object *
StringObject_new_const(Id s) {
  Object *o = malloc(sizeof(*o));
  o->ref = 0;
  o->t = ConstStringObject;
  o->c = s;
  return o;
}

static Object *
StringObject_new(const char *s) {
  return StringObject_new_const((Id){.s = s, .len = strlen(s)});
}

static Object *
FunctionJs_new(JsFun j) {
  Object *o = malloc(sizeof(*o));
  o->ref = 0;
  o->t = FunctionJs;
  o->j = j;
  return o;
}

static Object *
FunctionNative_new(Native f) {
  Object *o = malloc(sizeof(*o));
  o->ref = 0;
  o->t = FunctionNative;
  o->f = f;
  return o;
}

#ifdef NEEDLEAK
static
#endif
Object *
MapObject_new(void) {
  Object *o = malloc(sizeof(*o));
  o->ref = 0;
  o->t = MapObject;
  o->m = 0;
  return o;
}

static Object *
ArrayObject_new(void) {
  Object *o = malloc(sizeof(*o));
  o->ref = 0;
  o->t = ArrayObject;
  o->m = 0;
  return o;
}

static void
Map_set(List **list, char *key, Object *value) {
  if (!list) {
    return;
  }
  List *it = *list;
  while (it && it->key) {
    if (!strcmp(key, it->key)) {
      Object_free(it->value);
      it->value = Object_ref(value);
      mfree(key);
      return;
    }
    it = it->next;
  }
  *list = List_new(*list, key, Object_ref(value));
}

static void
Map_set_const(List **list, const char *s, Object *value) {
  Map_set(list, strdup(s), value);
}

static void
Map_set_id(List **list, Id *id, Object *value) {
  Map_set(list, strndup(id->s, id->len), value);
}

static int
strncmpEq(Id id, const char *s) {
  return !strncmp(id.s, s, id.len) && !s[id.len];
}

static List * __attribute__((pure))
Map_get(List *list, Id id) {
  List *it = list;
  while (it && it->key && it->value) {
    if (strncmpEq(id, it->key)) {
      return it;
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
  return Map_get(it->value->m, id);
}

static List *
Map_get_str(List *list, Str s) {
  return Map_get(list, (Id){.s = s.s, .len = s.len});
}

static int
List_length(List *list) {
  int n = 0;
  for (; list; list = list->next, n++) {}
  return n;
}

static int
isTrue(Object *o) {
  return ((o->t == IntObject) && o->i) || (((o->t == StringObject) || (o->t == ConstStringObject)) && o->c.len) || (o->t == MapObject) || (o->t == ArrayObject) || (o->t == FunctionJs) || (o->t == FunctionNative);
}
// TODO throw exception

static Object *
Object_toString(Object *o) {
  switch (o->t) {
    case IntObject: {
      char *s = malloc(16);
      int len = snprinti(s, 16, o->i);
      if ((len > 0) && (len < 16)) {
        return StringObject_new_str((Str){.s = s, .len = (size_t)len});
      } else {
        mfree(s);
      }
      break;
    }

    case UndefinedObject:
      return StringObject_new("undefined");

    case StringObject:
      return Object_ref(o);

    case ConstStringObject:
      return Object_ref(o);

    case MapObject:
      return StringObject_new("Object");

    case ArrayObject:
      return StringObject_new("Array");

    case FunctionJs:
      return StringObject_new("Function");

    case FunctionNative:
      return StringObject_new("Native");

    default: {}
  }
  return 0;
}

static Object *
String_concat(Object *t1, Object *t2) {
  if (((t1->t != StringObject) && (t1->t != ConstStringObject)) ||
    ((t2->t != StringObject) && (t2->t != ConstStringObject))) {
    return 0;
  }
  const size_t n = t1->s.len;
  const size_t m = t2->s.len;
  char *s = malloc(n + m + 1);
  strncpy(s, t1->s.s, n);
  strncpy(s + n, t2->s.s, m + 1);
  return StringObject_new_str((Str){.s = s, .len = n + m});
}

static void
putsn(const char *s, size_t len) {
  while (len-- && *s) {
    fputc(*(s++), stdout);
  }
}


static int
accept(Parser *p, char c) {
  if ((p->prog < p->progEnd) && (*p->prog == c)) {
    p->prog++;
    return 1;
  }
  return 0;
}

static int
expect(Parser *p, char c) {
  if (accept(p, c)) {
    return 1;
  }
  p->parseErr = "expected different character";
  p->parseErrChar = c;
  return 0;
}

static int
skipWs(Parser *p) {
  int ret = 0;
  while (1) {
    while ((p->prog < p->progEnd) && ((*p->prog == ' ') || (*p->prog == '\n'))) {
      p->prog++;
      ret = 1;
    }
    if ((p->progEnd - p->prog < 2) || (*p->prog != '/')) {
      break;
    }
    if (p->prog[1] == '/') {
      p->prog += 2;
      ret = 1;
      while ((p->prog < p->progEnd) && (*p->prog != '\n')) {
        p->prog++;
      }
    } else if (p->prog[1] == '*') {
      p->prog += 2;
      ret = 1;
      while (p->prog < p->progEnd) {
        if ((p->progEnd - p->prog >= 2) && (*p->prog == '*') && (p->prog[1] == '/')) {
          p->prog += 2;
          break;
        }
        p->prog++;
      }
    } else {
      break;
    }
  }
  return ret;
}

static int
acceptWs(Parser *p, char c) {
  skipWs(p);
  return accept(p, c);
}

static int
expectWs(Parser *p, char c) {
  skipWs(p);
  return expect(p, c);
}

static int
parseId(Parser *p, Id *id) {
  skipWs(p);
  const char *s = p->prog;
  char c = s < p->progEnd ? *s : 0;
  if ((c == '_') || ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))) {
    do {
      s++;
      c = s < p->progEnd ? *s : 0;
    } while ((c == '_') || ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')));
  }
  size_t len = off_t2size_t(s - p->prog);
  if (len) {
    id->s = p->prog;
    id->len = len;
    p->prog += len;
    return 1;
  }
  return 0;
}

static Object *
parseIntLit(Parser *p) {
  skipWs(p);

  const char *s = p->prog;
  char c = s < p->progEnd ? *s : 0;
  if (c == '-') {
    s++;
    c = s < p->progEnd ? *s : 0;
  }
  while ((c >= '0') && (c <= '9')) {
    s++;
    c = s < p->progEnd ? *s : 0;
  }
  size_t len = off_t2size_t(s - p->prog);
  if (!len || (c == '_') || (c == '.') || ((c >= 'A' && (c <= 'Z'))) || ((c >= 'a') && (c <= 'z'))) {
    return 0;
  }

  if (p->nest) {
    p->prog = s;
    return &undefinedObject;
  }
  int x;
  if (s < p->progEnd) {
    x = atoi(p->prog);
  } else {
    char *t = strndup(p->prog, len);
    x = atoi(t);
    free(t);
  }
  p->prog = s;
  return IntObject_new(x);
}

static Object *
parseStringLit(Parser *p) {
  skipWs(p);
  char c;
  if (!acceptWs(p, c = '\'') && !acceptWs(p, c = '"')) {
    return 0;
  }
  Id id = {.s = p->prog, .len = 0};
  while ((p->prog < p->progEnd) && (*p->prog != c) && (*p->prog != '\\')) {
    p->prog++;
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
parseIndex(Parser *p) {
  skipWs(p);
  if (accept(p, '[')) {
    Object *o = parseExpr(p);
    if (o) {
      if (expectWs(p, ']')) {
        return o;
      }
      Object_free(o);
    }
  } else if (accept(p, '.')) {
    Id id;
    if (parseId(p, &id)) {
      return StringObject_new_const(id);
    }
    p->parseErr = "expecting field name";
  }
  return 0;
}

static Object *
parseStatements(Parser *p);

static Object *
parseBody(Parser *p);

static Object *
parseFunction(Parser *p, Object *arg) {
  if (!acceptWs(p, ')')) {
    Id id;
    if (!parseId(p, &id)) {
      return 0;
    }
    if (!expectWs(p, ')')) {
      return 0;
    }
    if (!p->nest) {
      Map_set_id(&p->vars->m, &id, arg ? arg : &undefinedObject);
    }
  }
  return parseBody(p);
}

static Object *
parseRHS(Parser *p, List **parent, Object *key, List *e, Object *got) {
  skipWs(p);

  const char *prog = p->prog;
  if (accept(p, '=') && !accept(p, '=')) {
    Object *r = 0;
    if ((r = parseExpr(p))) {
      if (!p->nest) {
        if (e) {
          Object_free(e->value);
          e->value = Object_ref(r);
        } else {
          *parent = List_new(*parent, strndup(key->s.s, key->s.len), Object_ref(r));
        }
        if (getCycles(r)) {
          if (e) {
            e->value = key;
          } else {
            (*parent)->value = key;
          }
          Object_free(r);
          Object_free(r);
          return setRunError(p, "reference cycles unsupported", 0);
        }
      }
    }
    Object_free(key);
    return r;
  }
  p->prog = prog;

  Object *o = e ? Object_ref(e->value) : got ? got : &undefinedObject;
  Object_free(key);
  Object *arg = 0;
  if (accept(p, '(')) {
    if (acceptWs(p, ')') || ((arg = parseExpr(p)) && expectWs(p, ')'))) {
      if (p->nest) {
        Object_free(o);
        return &undefinedObject;
      }
      if (o->t == FunctionJs) {
        const char *ret = p->prog;
        p->prog = o->j.cs;
        Object *caller = p->vars;
        p->vars = MapObject_new();
        Map_set_const(&p->vars->m, "", o->j.scope);
        Object_free(o);

        Object *res = parseFunction(p, arg);
        if (arg) {
          Object_free(arg);
        }
        Object_free(p->vars);
        p->vars = caller;

        if (res) {
          p->prog = ret;
          return res;
        }
        if (p->ret) {
          p->prog = ret;
          res = p->ret;
          p->ret = 0;
          return res;
        }
      } else if (o->t == FunctionNative) {
        Object *as = MapObject_new();
        if (arg) {
          Map_set_const(&as->m, "0", arg);
          Object_free(arg);
        }
        Object *res = (*o->f)(p, as);
        Object_free(o);
        Object_free(as);
        return res;
      } else {
        if (arg) {
          Object_free(arg);
        }
        Object_free(o);
        return setRunError(p, "not a function", 0);
      }
    } else {
      if (arg) {
        Object_free(arg);
      }
      Object_free(o);
    }
    return 0;
  }

  return o;
}

static Object *
parseSTerm(Parser *p, Id *id) {
  Object *i = parseIndex(p);
  if (!i && (p->err || p->parseErr)) {
    return 0;
  }
  if (p->nest) {
    if (i) {
      Object_free(i);
    }
    return parseRHS(p, 0, &undefinedObject, 0, 0);
  }

  List *m;
  if (!(m = Map_get(p->vars->m, *id))) {
    if (i) {
      Object_free(i);
    }
    return setRunError(p, "undefined variable", id);
  }
  if (!i) {
    return parseRHS(p, &p->vars->m, &undefinedObject, m, 0);
  }

  if ((m->value->t != MapObject) && (m->value->t != ArrayObject)) {
    Object_free(i);
    return setRunError(p, "not indexable", id);
  }
  Object *is = Object_toString(i);
  Object_free(i);
  if (!is) {
    return setRunError(p, "can't convert to string", id);
  }

  List *e = Map_get_str(m->value->m, is->s);
  if (!e && (m->value->t == ArrayObject) && strncmpEq(is->c, "length")) {
    return parseRHS(p, &m->value->m, is, 0, IntObject_new(List_length(m->value->m)));
  }
  return parseRHS(p, &m->value->m, is, e, 0);
}

static Object *
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
    }
    return setRunError(p, "can't instantiate class of", id);
  } else if (strncmpEq(*id, "undefined")) {
    return &undefinedObject;
  } else if (strncmpEq(*id, "null")) {
    return &nullObject;
  }

  return parseSTerm(p, id);
}

static Object *
parseLTerm(Parser *p) {
  Object *o;
  if ((o = parseIntLit(p))) {
    return o;
  } else if ((o = parseStringLit(p))) {
    return o;
  } else if (acceptWs(p, '!')) {
    Id id;
    if (parseId(p, &id) && (o = parseSTerm(p, &id))) {
      Object *b = IntObject_new(!isTrue(o));
      Object_free(o);
      return b;
    }
  }
  p->parseErr = "expected keyword or identifier";
  return 0;
}

static Object *
parseTerm(Parser *p) {
  Id id;
  if (parseId(p, &id)) {
    return parseITerm(p, &id);
  } else {
    p->parseErr = 0;
    return parseLTerm(p);
  }
}

static int
isString(Object *o) {
  return (o->t == StringObject) || (o->t == ConstStringObject);
}

static int
isStringEq(Object *a, Object *b) {
  return isString(a) && isString(b) && (a->c.len == b->c.len) && (!strncmp(a->c.s, b->c.s, a->c.len));
}

static Object *
parseEExpr(Parser *p, Object *t1) {
  if (!t1) {
    return 0;
  }
  skipWs(p);

  char op;
  if (accept(p, op = '+')) {
  } else if (accept(p, op = '-')) {
  } else if (accept(p, op = '|')) {
  } else if (accept(p, op = '&')) {
  } else if (accept(p, op = '<')) {
  } else if (accept(p, '>')) {
    if (!expect(p, '>')) {
      Object_free(t1);
      return 0;
    }
    op = 'R';
  } else if (accept(p, '=')) {
    if (!expect(p, '=') || !expect(p, '=')) {
      Object_free(t1);
      return 0;
    }
    op = 'E';
  } else {
    return t1;
  }

  Object *t2 = parseTerm(p);
  if (!t2) {
    Object_free(t1);
    return 0;
  }

  if (p->nest) {
    Object_free(t2);
    return t1;
  }

  if ((t1->t == IntObject) && (t1->t == t2->t)) {
    int x = t1->i;
    int y = t2->i;
    Object_free(t1);
    Object_free(t2);
    switch (op) {
      case '+':
        return IntObject_new(x + y);
      case '-':
        return IntObject_new(x - y);
      case '|':
        return IntObject_new(x | y);
      case '&':
        return IntObject_new(x & y);
      case 'R':
        return IntObject_new(x >> y);
      case '<':
        return IntObject_new(x < y);
      case 'E':
        return IntObject_new(x == y);
      default: {}
    }
    return 0;
  } else if (op == 'E') {
    int b = isStringEq(t1, t2) || ((t1->t == NullObject) && (t2->t == NullObject)) || ((t1->t == UndefinedObject) && (t2->t == UndefinedObject));
    Object_free(t1);
    Object_free(t2);
    return IntObject_new(b);
  } else if (op == '+') {
    Object *s = 0;
    if (isString(t1)) {
      s = t2;
      t2 = Object_toString(s);
      Object_free(s);
    } else if (isString(t2)) {
      s = t1;
      t1 = Object_toString(s);
      Object_free(s);
    }
    if (s) {
      Object *o = String_concat(t1, t2);
      Object_free(t1);
      Object_free(t2);
      return o;
    }
  }
  Object_free(t1);
  Object_free(t2);
  return setRunError(p, "unknown operand types for expression", 0);
}

static Object *
parseIExpr(Parser *p, Id *id) {
  return parseEExpr(p, parseITerm(p, id));
}

static Object *
parseLExpr(Parser *p) {
  return parseEExpr(p, parseLTerm(p));
}

static Object *
parseExpr(Parser *p) {
  Id id;
  if (parseId(p, &id)) {
    return parseIExpr(p, &id);
  } else {
    p->parseErr = 0;
    return parseLExpr(p);
  }
}

static Object *
parseWhile(Parser *p) {
  if (!expectWs(p, '(')) {
    return 0;
  }

  const char *begin = p->prog;
  Object *o;
  int cond = 1;
  do {
    p->prog = begin;
    if (!(o = parseExpr(p)) || !expect(p, ')')) {
      return 0;
    }
    if (!p->nest) {
      if (!isTrue(o)) {
        p->nest++;
        cond = 0;
      }
    }
    Object_free(o);
    if (!(o = parseBody(p))) {
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
parseIf(Parser *p) {
  Object *o;
  if (!expectWs(p, '(') || !(o = parseExpr(p))) {
    return 0;
  }
  int cond = !p->nest && isTrue(o);
  if (!cond) {
    p->nest++;
  }
  Object_free(o);
  if (!expectWs(p, ')') || !(o = parseBody(p))) {
    return 0;
  }
  Object_free(o);
  if (!cond) {
    p->nest--;
  }

  Id id;
  if (!parseId(p, &id)) {
    return &undefinedObject;
  }

  if (!strncmpEq(id, "else")) {
    return 0;
  }
  if (cond) {
    p->nest++;
  }
  if (!(o = parseBody(p))) {
    return 0;
  }
  Object_free(o);
  if (cond) {
    p->nest--;
  }
  return &undefinedObject;
}

static Object *
parseStatement(Parser *p) {
  Object *o = 0;
  Id id;
  p->needSemicolon = 1;

  if (parseId(p, &id)) {
    if (strncmpEq(id, "if")) {
      o = parseIf(p);
    } else if (strncmpEq(id, "while")) {
      o = parseWhile(p);
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
          Map_set_const(&p->vars->m, "", caller);
          Map_set_id(&p->vars->m, &id, p->thrw);
        }
        Object_free(p->thrw);
        p->thrw = 0;
        if ((o = parseBody(p))) {
          Object_free(o);
          o = &undefinedObject;
        }
        if (caller) {
          Object_free(p->vars);
          p->vars = caller;
        }
      } else {
        p->nest++;
        if ((o = parseBody(p))) {
          Object_free(o);
          o = &undefinedObject;
        }
        p->nest--;
      }
    } else if (strncmpEq(id, "var")) {
      if (parseId(p, &id)) {
        if (!p->nest) {
          Map_set_id(&p->vars->m, &id, &undefinedObject);
        }
        o = &undefinedObject;
      }
    } else if (strncmpEq(id, "function")) {
      if (parseId(p, &id) && expectWs(p, '(')) {
        if (!p->nest) {
          Object *f = FunctionJs_new((JsFun){.cs = p->prog, .scope = Object_clone(p->vars)});
          Map_set_id(&p->vars->m, &id, f);
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
    p->parseErr = 0;
    o = parseLExpr(p);
  }
  return o;
}

static int
haveMore(Parser *p) {
  return !p->parseErr && (p->prog < p->progEnd);
}

static int
parseRequiredSemicolon(Parser *p) {
  skipWs(p);
  if (p->needSemicolon) {
    if (expect(p, ';')) {
      p->needSemicolon = 0;
    } else {
      return 0;
    }
  } else {
    if (accept(p, ';')) {
      p->needSemicolon = 0;
    }
  }
  return 1;
}

static void
parseOptionalSemicolon(Parser *p) {
  if (acceptWs(p, ';')) {
    p->needSemicolon = 0;
  }
}

static Object *
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
parseBody(Parser *p) {
  p->needSemicolon = 0;
  if (!expectWs(p, '{')) {
    return 0;
  }
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
document_write(Parser *p, Object *o) {
  List *l = Map_get(o->m, (Id){.s = "0", .len = 1});
  Object *e = l ? l->value : &undefinedObject;
  Object *os = Object_toString(e);
  if (os) {
    putsn(os->s.s, os->s.len);
    Object_free(os);
  } else {
    puts("?");
  }
  return &undefinedObject;
}

Parser *
Parser_new(void) {
  Parser *p = malloc(sizeof(*p));
  p->vars = MapObject_new();

  Object *doc = MapObject_new();
  Object *w = FunctionNative_new(&document_write);
  Map_set_const(&doc->m, "write", w);
  Object_free(w);
  Map_set_const(&p->vars->m, "document", doc);
  Object_free(doc);
  return p;
}

static void
showProg(Parser *p) {
  const size_t left = off_t2size_t(p->progEnd - p->prog);
  putsn(p->prog, left < 16 ? left : 16);
  puts("");
}

int
Parser_eval(Parser *p, const char *prog, size_t len) {
  if (!prog) {
    return 0;
  }
  p->prog = prog;
  p->progEnd = prog + len;
  p->parseErr = 0;
  p->parseErrChar = 0;
  p->err = 0;
  p->nest = 0;
  p->ret = 0;
  p->thrw = 0;
  p->needSemicolon = 0;

  Object *o = parseStatements(p);
  if (o) {
    int ret;
    if (p->thrw) {
      Object *os = Object_toString(p->thrw);
      Object_free(p->thrw);
      p->thrw = 0;
      puts("runtime error: exception");
      putsn(os->s.s, os->s.len);
      Object_free(os);
      puts("");
      ret = -2;
    } else {
      ret = o->t == IntObject ? o->i : isTrue(o);
    }
    Object_free(o);
    return ret;
  }

  if (p->ret) {
    Object_free(p->ret);
    p->ret = 0;
    puts("runtime error: return outside function");
    return -2;
  }

  if (p->thrw) {
    Object_free(p->thrw);
    p->thrw = 0;
  }
  if (p->parseErr || !p->err) {
    fputs("parse error: ", stdout);
    if (p->parseErr) {
      fputs(p->parseErr, stdout);
      if (p->parseErrChar) {
        fputs(" ( '", stdout);
        putchar(p->parseErrChar);
        fputs("' )", stdout);
      }
    }
    puts("");
    showProg(p);
    return -1;
  } else {
    fputs("runtime error: ", stdout);
    if (p->err) {
      fputs(p->err, stdout);
      if (p->errName.s) {
        fputs(" - ", stdout);
        putsn(p->errName.s, p->errName.len);
      }
    }
    puts("");
    showProg(p);
    return -2;
  }
}
