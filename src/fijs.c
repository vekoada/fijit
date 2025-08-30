#include "fijs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ALPHABET_SIZE 256
#define HASH_TABLE_SIZE 1024

typedef struct {
    char*   memory;
    size_t  total_size;
    size_t  offset;
} Arena;

typedef struct LocationNode {
    size_t location;
    struct LocationNode* next;
} LocationNode;

typedef struct HashNode {
    const char* key;
    LocationNode* locations;
    size_t location_count;
    struct HashNode* next;
} HashNode;

typedef struct {
    HashNode** buckets;
    int size;
} HashTable;

typedef struct {
    size_t frequency[ALPHABET_SIZE];
    size_t* offsets;
    void* all_locations;
    uint8_t location_size_bytes;
} CharIndex;

typedef struct FIJS_Index_s {
    const char* text;
    size_t      text_len;
    CharIndex*  index;
    Arena       master_arena;
} FIJS_Index;

static void arena_init(Arena* arena, size_t size) {
    arena->memory = malloc(size);
    if (!arena->memory) {
        arena->total_size = 0;
        arena->offset = 0;
        return;
    }
    arena->total_size = size;
    arena->offset = 0;
}

static void* arena_alloc(Arena* arena, size_t size) {
    size_t aligned_size = (size + 7) & ~7;
    if (arena->offset + aligned_size > arena->total_size) {
        return NULL;
    }
    void* ptr = arena->memory + arena->offset;
    arena->offset += aligned_size;
    return ptr;
}

static void arena_free(Arena* arena) {
    free(arena->memory);
    arena->memory = NULL;
    arena->total_size = 0;
    arena->offset = 0;
}

static unsigned int hash_function(const char* key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_TABLE_SIZE;
}

static void hash_table_insert(HashTable* ht, const char* key, size_t location, Arena* arena) {
    unsigned int index = hash_function(key);
    HashNode* current = ht->buckets[index];
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) {
            LocationNode* newLoc = arena_alloc(arena, sizeof(LocationNode));
            if (!newLoc) return;
            newLoc->location = location;
            newLoc->next = current->locations;
            current->locations = newLoc;
            current->location_count++;
            return;
        }
        current = current->next;
    }
    HashNode* newNode = arena_alloc(arena, sizeof(HashNode));
    if (!newNode) return;
    newNode->key = key;
    newNode->locations = arena_alloc(arena, sizeof(LocationNode));
    if (!newNode->locations) return;
    newNode->locations->location = location;
    newNode->location_count = 1;
    newNode->locations->next = NULL;
    newNode->next = ht->buckets[index];
    ht->buckets[index] = newNode;
}

static HashTable* create_hash_table(Arena* arena) {
    HashTable* ht = arena_alloc(arena, sizeof(HashTable));
    if (!ht) return NULL;
    ht->size = HASH_TABLE_SIZE;
    ht->buckets = arena_alloc(arena, ht->size * sizeof(HashNode*));
    if (!ht->buckets) return NULL;
    memset(ht->buckets, 0, ht->size * sizeof(HashNode*));
    return ht;
}

static bool calculate_frequencies(const char* text, size_t text_len, size_t* frequency) {
    memset(frequency, 0, ALPHABET_SIZE * sizeof(size_t));
    
    for (size_t i = 0; i < text_len; ++i) {
        frequency[(unsigned char)text[i]]++;
    }
    
    return true;
}

static bool build_offsets(const size_t* frequency, size_t* offsets) {
    size_t current_offset = 0;
    for (int i = 0; i < ALPHABET_SIZE; ++i) {
        offsets[i] = current_offset;
        current_offset += frequency[i];
    }
    offsets[ALPHABET_SIZE] = current_offset;
    
    return true;
}

static bool populate_locations(const char* text, size_t text_len, uint8_t location_size_bytes, void* all_locations, const size_t* offsets) {
    size_t* write_positions = malloc(ALPHABET_SIZE * sizeof(size_t));
    if (!write_positions) return false;
    
    memcpy(write_positions, offsets, ALPHABET_SIZE * sizeof(size_t));

    for (size_t i = 0; i < text_len; ++i) {
        unsigned char c = text[i];
        size_t write_pos = write_positions[c];
        
        switch (location_size_bytes) {
            case 2: ((uint16_t*)all_locations)[write_pos] = (uint16_t)i; break;
            case 4: ((uint32_t*)all_locations)[write_pos] = (uint32_t)i; break;
            case 8: ((uint64_t*)all_locations)[write_pos] = (uint64_t)i; break;
        }
        write_positions[c]++;
    }

    free(write_positions);
    return true;
}

static CharIndex* build_char_index(const char* text, size_t text_len, uint8_t location_size_bytes, Arena* arena) {
    CharIndex* char_index = arena_alloc(arena, sizeof(CharIndex));
    if (!char_index) return NULL;
    
    char_index->location_size_bytes = location_size_bytes;
    
    if (!calculate_frequencies(text, text_len, char_index->frequency)) {
        return NULL;
    }
    
    char_index->offsets = arena_alloc(arena, (ALPHABET_SIZE + 1) * sizeof(size_t));
    if (!char_index->offsets) return NULL;
    
    if (!build_offsets(char_index->frequency, char_index->offsets)) {
        return NULL;
    }
    
    char_index->all_locations = arena_alloc(arena, text_len * location_size_bytes);
    if (!char_index->all_locations) return NULL;
    
    if (!populate_locations(text, text_len, location_size_bytes, 
                           char_index->all_locations, char_index->offsets)) {
        return NULL;
    }
    
    return char_index;
}

static inline int find_rarest_char(const char* pattern, size_t pattern_len, const FIJS_Index* index, unsigned char* rare_char) {
    size_t min_freq = (size_t)-1;
    int rare_char_offset = -1;
    
    for (size_t j = 0; j < pattern_len; ++j) {
        unsigned char c = pattern[j];
        if (index->index->frequency[c] < min_freq) {
            min_freq = index->index->frequency[c];
            *rare_char = c;
            rare_char_offset = j;
        }
    }

return (min_freq == 0) ? -1 : rare_char_offset;
}

static inline size_t get_location_at_index(const FIJS_Index* index, size_t k) {
    switch (index->index->location_size_bytes) {
        case 2: return ((uint16_t*)index->index->all_locations)[k];
        case 4: return ((uint32_t*)index->index->all_locations)[k];
        case 8: return ((uint64_t*)index->index->all_locations)[k];
        default: return 0;
    }
}

static void search_single_pattern(const FIJS_Index* index, const char* pattern, HashTable* results, Arena* result_arena) {
    size_t pattern_len = strlen(pattern);
    if (pattern_len == 0 || pattern_len > index->text_len) {
        return;
    }
    unsigned char rare_char;
    int rare_char_offset = find_rarest_char(pattern, pattern_len, index, &rare_char);
    if (rare_char_offset == -1) {
        return;
    }
    size_t start_offset = index->index->offsets[rare_char];
    size_t end_offset = index->index->offsets[rare_char + 1];
        for (size_t k = start_offset; k < end_offset; ++k) {
            size_t location_of_rare_char = get_location_at_index(index, k);
            
            long long start_pos_ll = (long long)location_of_rare_char - rare_char_offset;
            if (start_pos_ll >= 0) {
                size_t start_pos = (size_t)start_pos_ll;
                if (memcmp(index->text + start_pos, pattern, pattern_len) == 0) {
                    hash_table_insert(results, pattern, start_pos, result_arena);
                }
            }
        }
    }

static HashTable* search_into_hashtable(FIJS_Index* index, const char** patterns, int num_patterns, Arena* result_arena) {
    HashTable* results = create_hash_table(result_arena);
    if (!results) return NULL;
    for (int i = 0; i < num_patterns; ++i) {
        search_single_pattern(index, patterns[i], results, result_arena);
    }
    return results;
}

FIJS_Index* fijs_index_create(const char* text) {
    if (!text) return NULL;

    size_t text_len = strlen(text);
    if (text_len == 0) return NULL;

    uint8_t location_size_bytes;
    if (text_len < 65536) {
        location_size_bytes = sizeof(uint16_t);
    } else if (text_len < 4294967296UL) {
        location_size_bytes = sizeof(uint32_t);
    } else {
        location_size_bytes = sizeof(uint64_t);
    }

    size_t arena_size = sizeof(CharIndex) + (ALPHABET_SIZE + 1) * sizeof(size_t) + text_len * location_size_bytes;

    FIJS_Index* index = malloc(sizeof(FIJS_Index));
    if (!index) return NULL;

    arena_init(&index->master_arena, arena_size);
    if (!index->master_arena.memory) {
        free(index);
        return NULL;
    }

    index->text = strdup(text);
    if (!index->text) {
        arena_free(&index->master_arena);
        free(index);
        return NULL;
    }
    index->text_len = text_len;
    index->index = build_char_index(text, text_len, location_size_bytes, &index->master_arena);

    if (!index->index) {
        arena_free(&index->master_arena);
        free(index);
        return NULL;
    }

    return index;
}

void fijs_index_destroy(FIJS_Index* index) {
    if (index) {
        free((void*)index->text); 
        arena_free(&index->master_arena);
        free(index);
    }
}

static size_t calculate_total_size(HashTable* ht) {
    size_t total = 0;
    for (int i = 0; i < ht->size; ++i) {
        for (HashNode* node = ht->buckets[i]; node; node = node->next) {
            total += sizeof(FIJS_Result) + node->location_count * sizeof(size_t);
        }
    }
    return total;
}

static void populate_results(HashTable* ht, char* result_block, FIJS_Result** head) {
    char* ptr = result_block;
    FIJS_Result* prev = NULL;
    
    for (int i = 0; i < ht->size; ++i) {
        for (HashNode* node = ht->buckets[i]; node; node = node->next) {
            FIJS_Result* current = (FIJS_Result*)ptr;
            ptr += sizeof(FIJS_Result);
            
            if (!*head) *head = current;
            if (prev) prev->next = current;
            
            current->pattern = node->key;
            current->count = node->location_count;
            current->locations = (size_t*)ptr;
            ptr += node->location_count * sizeof(size_t);
            
            size_t idx = 0;
            for (LocationNode* loc = node->locations; loc; loc = loc->next) {
                current->locations[idx++] = loc->location;
            }
            
            prev = current;
        }
    }
    if (prev) prev->next = NULL;
}

FIJS_Result* fijs_search(FIJS_Index* index, const char** patterns, int num_patterns) {
    Arena temp_arena;
    arena_init(&temp_arena, 1024 * 64);
    if (!temp_arena.memory) return NULL;
    
    HashTable* ht = search_into_hashtable(index, patterns, num_patterns, &temp_arena);
    if (!ht) {
        arena_free(&temp_arena);
        return NULL;
    }
    
    size_t total_size = calculate_total_size(ht);
    if (total_size == 0) {
        arena_free(&temp_arena);
        return NULL;
    }
    
    char* result_block = malloc(total_size);
    if (!result_block) {
        arena_free(&temp_arena);
        return NULL;
    }
    
    FIJS_Result* head = NULL;
    populate_results(ht, result_block, &head);
    
    arena_free(&temp_arena);
    return head;
}

void fijs_results_free(FIJS_Result* results) {
    free(results);
}