# Aster Language — Development Notes

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
- Value system (numbers/strings/bools/null/functions)\n+- Environments for scoping (`Env`)\n+- Interpreter that executes the AST\n*** End Patch}]} />
