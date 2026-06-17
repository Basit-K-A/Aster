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
