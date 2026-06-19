#ifndef ASTER_DEBUG_H
#define ASTER_DEBUG_H

#include "chunk.h"

/* Prints a human-readable disassembly of a bytecode chunk */
void disassemble(Chunk* chunk, const char* name);

#endif /* ASTER_DEBUG_H */
