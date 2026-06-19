# Aster Language — Development Notes

## Project layout (modular)

| Module | Files | Responsibility |
|--------|-------|----------------|
| Token | `token.h` | Token type definitions |
| Lexer | `lexer.h`, `lexer.c` | Tokenization |
| AST | `ast.h`, `ast.c` | AST types, `printAst`, `freeAst` |
| Parser | `parser.h`, `parser.c` | Recursive-descent parser, `parse()` |
| Value | `value.h`, `value.c` | Runtime values and functions |
| Env | `env.h`, `env.c` | Lexical scoping |
| Interpreter | `interpreter.h`, `interpreter.c` | Tree-walk execution |
| Chunk | `chunk.h`, `chunk.c` | Bytecode container and constant pool |
| Compiler | `compiler.h`, `compiler.c` | AST → bytecode compiler |
| Debug | `debug.h`, `debug.c` | Bytecode disassembler |
| Main | `main.c` | Entry point and tests |

Build with `make` (Unix) or:

```powershell
gcc -std=c17 -Wall -Wextra -c main.c lexer.c ast.c parser.c value.c env.c interpreter.c chunk.c compiler.c debug.c
gcc -std=c17 -Wall -Wextra -o aster main.o lexer.o ast.o parser.o value.o env.o interpreter.o chunk.o compiler.o debug.o -lm
```

## Phase 1 — Lexer (Complete)

### What was built
- Full tokenizer in `main.c` (single-file build)
- `TokenType` enum with literals, keywords, operators, delimiters, EOF, and ERROR
- `Token` struct with type, source pointer, length, and line number
- `Lexer` struct tracking scan position and current line
- `initLexer`, `nextToken`, and `printToken` functions
- Support for:
  - Numbers (integers and decimals)
  - Double-quoted strings
  - Identifiers and all specified keywords
  - Single- and multi-character operators (`==`, `!=`, `>=`, `<=`, `&&`, `||`)
  - Delimiters: `(){}[],;.`
  - Whitespace skipping and `//` line comments
  - Line number tracking

### Known limitations
- No escape sequences inside strings (e.g. `\n` is literal backslash + n)
- No block comments (`/* */`)
- No hex/octal/binary number literals
- Lexer only; no parser or execution yet

### Next phase (Phase 2 — Parser & AST)
- Recursive-descent parser over the token stream
- `AstNode` tagged union with pointer-based children
- `parse`, `printAst`, and `freeAst` functions
- Grammar for declarations, statements, and expressions

## Phase 2 — Parser & AST (Complete)

### What was built
- Recursive-descent parser added to `main.c` using the Phase 1 lexer
- Pointer-based `AstNode` tagged union with node-specific payloads
- `parse(const char* source)` entry point producing a program-level `NODE_BLOCK`
- Debug pretty printer `printAst(AstNode* node, int indent)`
- Recursive deallocator `freeAst(AstNode* node)` covering all node types

### Supported grammar
- Declarations: `let` var declarations, `function` declarations
- Statements: block, if/else, while, return, print, expression statements
- Expressions: assignment, logical `&&`/`||`, equality, comparison, term, factor, unary, call, primary

### Known limitations
- Basic error recovery; stops returning AST on first error
- `print` is parsed as a statement (not a normal function identifier) to match Phase 2 spec

### Next phase (Phase 3 — Tree-walk interpreter)
- Value system (numbers/strings/bools/null/functions)
- Environments for scoping (`Env`)
- Interpreter that executes the AST

## Phase 3 — Tree-walk Interpreter (Complete)

### What was built
- Runtime `Value` system: numbers, strings, booleans, null, and user-defined functions
- `Env` with `envCreate`, `envDefine`, `envSet`, `envGet`, and `envFree` for lexical scoping
- `Interpreter` struct with globals, error flag, and return-value control
- `interpret(AstNode*, Interpreter*, Env*)` dispatches all node types
- Tree-walk execution: binary/unary ops, assignment, control flow, `print`, function calls
- String concatenation via `+` when either operand is a string

### Phase 3 test results
- **Test A:** `print(x + y)` → `30`
- **Test B:** `greet("Aster")` → `Hello, Aster`
- **Test C:** `while` loop → `0`, `1`, `2`
- **Test D:** `if/else` → `yes`

### Known limitations
- No closures yet (functions use defining environment as parent at call time)
- `print` is a statement, not a first-class callable value
- Fixed-size environment tables (256 bindings per scope)

### Next phase (Phase 4 — Bytecode compiler)
- `Chunk` bytecode container with constant pool
- `compile(AstNode*, Compiler*)` AST → opcodes
- `disassemble(Chunk*, const char*)` for debugging

## Phase 4 — Bytecode Compiler (Complete)

### What was built
- `Chunk` with growable bytecode stream, constant pool, and line info
- `writeChunk`, `addConstant`, `freeChunk` in `chunk.c`
- Full opcode set per spec (globals, locals, jumps, loops, calls, halt)
- `compile(AstNode*, Compiler*)` walks AST and emits stack bytecode
- Jump patching for `if`/`while` and short-circuit `&&`/`||`
- `disassemble(Chunk*, const char*)` in `debug.c`

### Phase 4 test output (`let x = 5 + 10; print(x);`)
- `OP_CONST 5`, `OP_CONST 10`, `OP_ADD`, `OP_DEF_GLOBAL x`
- `OP_GET_GLOBAL x`, `OP_PRINT`, `OP_HALT`

### Known limitations
- User-defined functions not compiled yet (deferred to Phase 6)
- No VM execution yet (Phase 5)
- Global name constants may be duplicated in the constant pool

### Next phase (Phase 5 — Stack-based VM)
- `VM` struct with stack, call frames, globals
- `run(VM*)` dispatch loop executing `Chunk` bytecode
- All Phase 3 tests must pass through the VM*** End Patch}]} />
