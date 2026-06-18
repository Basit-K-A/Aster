/* Aster Language — entry point and Phase 3 regression tests */

#include <stdio.h>

#include "interpreter.h"

int main(void) {
    printf("=== Phase 3 Interpreter Tests ===\n\n");

    bool ok = true;

    ok = runSource(
        "let x = 10;\n"
        "let y = 20;\n"
        "print(x + y);\n",
        "Test A") && ok;

    ok = runSource(
        "function greet(name) { print(\"Hello, \" + name); }\n"
        "greet(\"Aster\");\n",
        "Test B") && ok;

    ok = runSource(
        "let i = 0;\n"
        "while (i < 3) { print(i); i = i + 1; }\n",
        "Test C") && ok;

    ok = runSource(
        "if (5 > 3) { print(\"yes\"); } else { print(\"no\"); }\n",
        "Test D") && ok;

    if (!ok) return 1;

    printf("=== Phase 3 complete. Run tests before continuing. ===\n");
    return 0;
}
