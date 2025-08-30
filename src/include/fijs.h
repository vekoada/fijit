#ifndef FIJS_H
#define FIJS_H

#include <stddef.h>

typedef struct FIJS_Index_s FIJS_Index;

typedef struct FIJS_Result {
    const char*         pattern;
    size_t              count;
    size_t*             locations;
    struct FIJS_Result* next;
} FIJS_Result;

FIJS_Index* fijs_index_create(const char* text);

void fijs_index_destroy(FIJS_Index* index);

FIJS_Result* fijs_search(FIJS_Index* index, const char** patterns, int num_patterns);

void fijs_results_free(FIJS_Result* results);

#endif