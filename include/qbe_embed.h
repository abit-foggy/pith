#ifndef PITH_QBE_EMBED_H
#define PITH_QBE_EMBED_H

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compile QBE SSA IR text from an open FILE* stream into assembly.
 * Returns a dynamically allocated heap string (caller must free()),
 * or NULL on failure.
 */
char *qbe_compile_file(FILE *inf, const char *source_name);

/*
 * Compile QBE SSA IR from an in-memory buffer into assembly.
 * Returns a dynamically allocated heap string (caller must free()),
 * or NULL on failure.
 */
char *qbe_compile_string(const char *ssa_text, size_t ssa_len);

#ifdef __cplusplus
}
#endif

#endif /* PITH_QBE_EMBED_H */
