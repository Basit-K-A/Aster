/* Aster Language — entry point and Phase 7 closure tests */

#include <stdio.h>

#include "vm.h"

int main(void) {
    printf("=== Phase 7 Closures ===\n\n");

    bool ok = runSourceVM(
        "function makeCounter() {\n"
        "  let count = 0;\n"
        "  function inc() { count = count + 1; return count; }\n"
        "  return inc;\n"
        "}\n"
        "let c = makeCounter();\n"
        "print(c());\n"
        "print(c());\n"
        "print(c());\n",
        "Phase 7 Test");

    if (!ok) return 1;

    printf("=== Phase 3/5/6 regression (VM) ===\n\n");

    ok = runSourceVM(
        "let x = 10;\n"
        "let y = 20;\n"
        "print(x + y);\n",
        "Test A") && ok;

    ok = runSourceVM(
        "function greet(name) { print(\"Hello, \" + name); }\n"
        "greet(\"Aster\");\n",
        "Test B") && ok;

    ok = runSourceVM(
        "let i = 0;\n"
        "while (i < 3) { print(i); i = i + 1; }\n",
        "Test C") && ok;

    ok = runSourceVM(
        "if (5 > 3) { print(\"yes\"); } else { print(\"no\"); }\n",
        "Test D") && ok;

    ok = runSourceVM(
        "function factorial(n) {\n"
        "if (n <= 1) { return 1; }\n"
        "return n * factorial(n - 1);\n"
        "}\n"
        "print(factorial(5));\n",
        "Test E (factorial)") && ok;

    if (!ok) return 1;

    printf("=== Phase 7 complete. Run tests before continuing. ===\n");
    return 0;
}
