/* Aster Language — entry point and Phase 6 function tests */

#include <stdio.h>

#include "vm.h"

int main(void) {
    printf("=== Phase 6 Functions & Call Stack ===\n\n");

    bool ok = runSourceVM(
        "function factorial(n) {\n"
        "if (n <= 1) { return 1; }\n"
        "return n * factorial(n - 1);\n"
        "}\n"
        "print(factorial(5));\n",
        "Phase 6 Test");

    if (!ok) return 1;

    printf("=== Phase 3/5 regression (VM) ===\n\n");

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

    if (!ok) return 1;

    printf("=== Phase 6 complete. Run tests before continuing. ===\n");
    return 0;
}
