/* Aster Language — entry point and Phase 5 VM tests */

#include <stdio.h>

#include "vm.h"

int main(void) {
    printf("=== Phase 5 VM Tests (Phase 3 regression) ===\n\n");

    bool ok = true;

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

    printf("=== Phase 5 complete. Run tests before continuing. ===\n");
    return 0;
}
