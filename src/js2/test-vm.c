#include "vm2.h"
#include <stdbool.h> // true false bool
#include <stdio.h> // printf puts
#include <string.h> // strlen

#ifdef SMALLBIN
#define SB(x,y) (x)
#else
#define SB(x,y) (y)
#endif

static bool _debug = true;
static unsigned int _errorCount = 0;

static int
test_case(const char *prog) {
  Parser *p = Parser_new();
  int ret = Parser_eval(p, prog, strlen(prog));
  Parser_free(p);
  return ret;
}

static void
t3(const char *code, int expect, const char *name) {
  const char *title = name ? name : code;
  if (_debug) {
    printf("=testing: %s\n", title);
  }
  int res = test_case(code);
  if (res != expect) {
    if (_debug) {
      printf(" -fail: got %d, expected %d\n", res, expect);
    } else {
      printf("fail: %s: got %d, expected %d\n", title, res, expect);
    }
    _errorCount++;
  }
}

static void
t(const char *code, int expect) {
  t3(code, expect, 0);
}

int
main(void) {
  t("function fn() { var i; i = 9 }; fn()", 0);
  t("function f() { return 9 }; var i; i = f()", 9);
  t("var i; function f() { return 7; i = 8 }; i = 9; f(); i", 9);
  t("var i; i = 9; function f() { i = 8 }; var g; g = f; i", 9);
  t("function f() { return 9 }; var m; m = new Object; m.a = f; m.a()", 9);
  t("function f() { function g() { return 9 }; return g() }; f()", 9);

  t3(
    "var a; var b; a = 24; b = 42; while (b < a) { a = a - b }; while (a < b) { while (a < b) { b = b - a }; while (b < a) { a = a - b } }; b",
    6,
    "while_gcd"
  );

  t("function f() { return 9 }; f()", 9);
  t("function f() { var i; i = 9; return i }; f()", 9);
  t("var i; i = 9; function f() { return i }; f()", 9);
  t("var i; i = 8; function f() { var i; i = 9; return i }; f()", 9);
  t("var i; i = 9; function f() { var i; i = 8; return i }; f(); i", 9);
  t("function f() { return 5 }; function g() { return 4 }; f() + g()", 9);
  t("function f() { return 9 }; f(8)", 9);
  t("function f(a) { return a + 4 }; f(5)", 9);
  t("function f(a) { return a === undefined }; f()", 1);
  t("function f() { 8 }; f() === undefined", 1);

  t("var s; s = 'a' + 'b'; s === 'ab'", 1);
  t("var s; s = 9 + 'x'; s === '9x'", 1);
  t("var s; s = 'x' + 9; s === 'x9'", 1);
  t("var i; i = 9", 9);
  t("var i; i = 0; i = !i", 1);
  t("var xY_0; xY_0 = 9", 9);
  t("var i; i = 9; i", 9);
  t("var i; i = 2 + 3", 5);
  t("var i; var j;  i = j = 9", 9);
  t("var i; var j; i = j = 9; i", 9);
  t("var i; var j; i = j = 9; j", 9);

  t("var m; m = new Object; m['a']", 0);
  t("var m; m = new Object; m['a'] = 9", 9);
  t("var m; m = new Object; m['a'] = 9; m['a']", 9);
  t("var m; m = new Object; m.a", 0);
  t("var m; m = new Object; m.a = 9; m.a", 9);
  t("var m; m = new Object; m.a === undefined", 1);
  t("var p; var q; p = new Object; q = p; p.a = 9; q.a", 9);
  t("var p; var q; p = new Object; q = new Object; q.p = p; q.p", 1);
  t("var p; var q; p = new Object; p.a = 9; q = new Object; q.p = p; p = q.p; p.a", 9);

  t("var i; i = 3; while (i) { i = i - 1 }; i", 0);
  t("var i; var j; var k; k = 0; i = 3; while (i) { j = 3; while (j) { k = k + 1; j = j - 1 }; i = i - 1 }; k", 9);
  t("var v; v = new Object; v[1", -1);
  t3(
    "var v; v = new Object; v[0] = 9; v[1] = 8; v[2] = 7; var s; s = ''; var i; i = 0; while (v[i]) { s = s + v[i]; i = i + 1 }; s === '987'",
    1,
    "test_while_concat"
  );

  t("var v; v = new Array; v[0] === undefined", 1);
  t("var v; v = new Array; v[0] = 9; v[0]", 9);
  t("var v; v = new Array; v.length === 0", 1);
  t("var v; v = new Array; v[0] = 8; v[1] = 7; v.length", 2);

  t("var u; while (0) { u = 9 }; u", 0);
  t("while (0) { 9 }", 0);

  t("var i; if (2) { i = 9 }; i", 9);
  t("var i; i = 2; if (i) { i = 9 }; i", 9);
  t("var i; i = 9; if (0) { i = 8 }; i", 9);
  t("var i; if (2) { i = 9 } else { i = 8 }; i", 9);
  t("var i; if (0) { i = 8 } else { i = 9 }; i", 9);
  t("var i; if (2) { i = 8; i = 9 } else { i = 6; i = 7 }; i", 9);
  t("var i; if (0) { i = 6; i = 7 } else { i = 8; i = 9 }; i", 9);
  t("var i; if (2) { if (3) { i = 9 } }; i", 9);
  t("var i; if (2) { if (3) { i = 9 } else { i = 8 } } else { i = 7 }; i", 9);

  t("undefined", 0);
  t("987654321", 987654321);
  t("-9", -9);
  t("5+-3", 2);
  t("2 + 3", 5);
  t("5 - 3", 2);
  t("3 | 6", 7);
  t("9 & 10", 8);
  t("14 >> 2", 3);
  t("2 < 3", 1);
  t("3 < 2", 0);
  t("2 === 2", 1);
  t("2 === 3", 0);
  t("undefined === undefined", 1);
  t("0 === undefined", 0);
  t("0 === '0'", 0);
  t("0 === ''", 0);
  t("0 === new Object", 0);
  t("0 === null", 0);
  t("null", 0);
  t("null === null", 1);
  t("null === undefined", 0);
  t("\"xa\" === \"xa\"", 1);
  t("\"xa\" === \"xb\"", 0);
  t("'xa' === 'xa'", 1);
  t("'xa' === 'xb'", 0);
  t("new Object", 1);
  t("new Array", 1);
  t("''", 0);
  t("'x'", 1);
  t("8; 9", 9);

  t("var i; i === undefined", 1);
  t("var i; i = 9", 9);

  t("var i; i = 9; i()", -2);
  t(".error", -1);
  t("u", -2);
  t("u = 9", -2);
  t("return 9", -2);
  t("function f() { var i; i = 9 }; f(); i", -2);
  t("function f() { var i; i = 9; return i }; f(); i", -2);
  t("document.write(9)", 0);
  t("document.write(9", -1);

  // TODO Object reference cycles
  t("var p; var q; p = new Object; q = new Object; p.q = q; q.p = p; q = p.q; q.p", SB(1, -2));
  t("var p; var q; var t; p = new Object; q = new Object; p.q = q; q.p = p; t = p.q; t.p", SB(1, -2));
  t("var p; var q; var t; p = new Object; q = new Object; p.q = q; q.p = p; q.i = 9; t = q.p; t = t.q; t.i", SB(9, -2));
  t("var p; p = new Object; p.p = p; p = p.p; p.p", SB(1, -2));
  t("var p; var q; p = new Object; q = new Object; p.p = p; p.q = q; q.p = p; q.q = q; q = p.q; q = q.p; q = q.p", SB(1, -2));
  t("var o; var p; var q; o = new Object; p = new Object; q = new Object; o.p = p; p.q = q; q.o = o; o = o.p; o = o.q; o.o", SB(1, -2));
  t("var o; var p; var q; o = new Object; p = new Object; q = new Object; o.p = p; o.q = q; p.q = q; p.o = o; q.p = p; q.o = o; o = o.p; o = o.q; o.o", SB(1, -2));

  // TODO function scope reference cycles
  t("var o; o = new Object; function f() { 9 }; o.f = f", SB(1, -2));
  t("function obj() { var o; o = new Object; o.i = 9; function get() { return o.i }; o.get = get; return o }; var p; p = obj(); p.get()", SB(9, -2));
  t("function obj() { var o; o = new Object; o.i = 8; function inc() { o.i = o.i + 1 }; o.inc = inc; function get() { return o.i }; o.get = get; return o }; var p; p = obj(); p.inc(); p.get()", SB(9, -2));

  // ok
  t("function obj() { var s; s = new Object; s.i = 9; function get() { return s.i }; var m; m = new Object; m.get = get; return m }; var p; p = obj(); p.get()", 9);
  t("function obj() { var s; s = new Object; s.i = 8; function get() { return s.i }; function inc() { s.i = s.i + 1 }; function ret() { var m; m = new Object; m.get = get; m.inc = inc; return m }; return ret() }; var p; p = obj(); p.inc(); p.get()", 9);
  t("function obj() { var s; s = new Object; s.i = 8; function get() { return s.i }; function inc() { s.i = s.i + 1 }; function ret() { var m; m = new Object; m.get = get; m.inc = inc; m.s = s; return m }; return ret() }; var p; p = obj(); p.inc(); p.get(); var t; t = p.s; t.i", 9);

  // TODO function scope by reference
  t("var i; function f() { i = 9 }; f(); i", SB(9, 0));
  t("var i; function f() { i = 9 }; f(); i = 8; f(); i", SB(9, 8));
  t("var i; function f() { i = 9 }; var m; m = new Object; m.a = f; m.a(); i", SB(9, 0));
  t(
    "var i; var j; i = 0; j = 0; function f() { function g() { i = i + 1; return 2; i = i + 8 }; j = g(); i = i + 4 }; f(); i + j",
    SB(7, 0));

  t("var i; try { i = 9 } catch (e) { i = 8 }; i", 9);
  t("var i; try { throw 4 } catch (e) { i = 9 }; i", 9);
  t("var i; var j; try { j = 4; throw 3; j = 2 } catch (e) { i = j + 5 }; i", 9);
  t("var i; try { throw 9 } catch (e) { i = e }; i", 9);
  t("var i; try { try { throw 8 } catch (e) { i = 9 } } catch (f) { i = 2 }; i", 9);
  t("var i; try { try { i = 2 } catch (e) { i = 3 }; throw 8 } catch (f) { i = 9 }; i", 9);
  t("function f() { throw 4 }; var i; try { f() } catch (e) { i = 9 }; i", 9);
  t("throw 5", -2);
  t("throw 5; 8", -2);
  t("var i; try { throw 4 } CATCH (e) { i = 2 }", -1);
  t("try { throw 4 } catch (e) { throw 5 }", -2);
  t("var i; try { try { throw 4 } catch (e) { throw 9 } } catch (e) { i = e }; i", 9);

  t("if (1) { 8 } 9", 9);
  t("if (1) { 8 } else { 7 } 9", 9);
  t("while (0) { 8 } 9", 9);
  t("function f() { 8 } 9", 9);
  t("try { 8 } catch (e) { 7 } 9", 9);

  t("7 8", -1);
  t("if (1) { 7 8 }", -1);
  t("if (1) { if (1) { 7 8 } }", -1);
  t("if (1) { if (1) { 6 } 7 8 }", -1);

  t("9;", 9);
  t("9; ", 9);
  t("9;\n ", 9);
  t("if (1) { 8; }", 0);
  t("if (1) { 8; } else { 7; }", 0);
  t("while (0) { 8; }", 0);
  t("function f() { 8; }", 0);
  t("try { 8; } catch (e) { 7; }", 0);
  t("if (1) { 8 }; else { 7 }", -2);
  t("try { 8 }; catch (e) { 7 }", -1);

  t("if (1) {}", 0);
  t("if (1) {} else {}", 0);
  t("while (0) {}", 0);
  t("function f() {}", 0);
  t("try {} catch (e) {}", 0);

  t("", 0);
  t("//", 0);
  t("// ", 0);
  t("//\n", 0);
  t("9//", 9);
  t("9 // x y\n", 9);
  t("//\n9", 9);
  t("// x\n9", 9);
  t("// x\n// y\n9", 9);
  t("var i; i = 9; // x y\n i", 9);
  t("9/**/", 9);
  t("9/* **/", 9);
  t("9/***/", 9);
  t("9/* x */", 9);
  t("9/* /* */", 9);
  t("9/* * / **/", 9);
  t("9 /* x \n y */\n", 9);
  t("/* */9", 9);
  t("/* */\n9", 9);
  t("// /* \n9", 9);
  t("/* // */9", 9);

  if (_errorCount) {
    printf("%d test(s) failed\n", _errorCount);
  } else {
    puts("All tests successful");
  }
  return _errorCount ? 1 : 0;
}
