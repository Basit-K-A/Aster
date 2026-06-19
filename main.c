/* Aster Language — entry point and Phase 4 bytecode test */

#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "debug.h"
#include "interpreter.h"
#include "parser.h"

/* Parses, compiles, and disassembles source for the Phase 4 test */
static bool runCompileTest(const char* source, const char* name) {
    AstNode* program = parse(source);
    if (!program) {
        fprintf(stderr, "Parse failed.\n");
        return false;
    }

    Chunk chunk;
    initChunk(&chunk);

    Compiler compiler;
    compiler.chunk = &chunk;
    compiler.hadError = false;

    compile(program, &compiler);
    freeAst(program);

    if (compiler.hadError) {
        fprintf(stderr, "Compile failed.\n");
        freeChunk(&chunk);
        return false;
    }

    disassemble(&chunk, name);
    printf("\n");
    freeChunk(&chunk);
    return true;
}

int main(void) {
    printf("=== Phase 4 Bytecode Compiler Test ===\n\n");

    if (!runCompileTest(
            "let x = 5 + 10;\n"
            "print(x);\n",
            "script")) {
        return 1;
    }

    printf("=== Phase 3 regression (interpreter) ===\n\n");

    bool ok = true;
    ok = runSource("let x = 10;\nlet y = 20;\nprint(x + y);\n", "Test A") && ok;
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

    printf("=== Phase 4 complete. Run tests before continuing. ===\n");
    return 0;
}
