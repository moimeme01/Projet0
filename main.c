
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct BlockHeader {
    size_t size;         // Taille du bloc utilisateur
    unsigned char free;  // 1 = libre, 0 = occupé
    size_t prev;         // Offset du bloc précédent dans MY_HEAP
    size_t next;         // Offset du bloc suivant dans MY_HEAP
} BlockHeader;

#define HEAP_SIZE 64000
unsigned char MY_HEAP[HEAP_SIZE];

void init_heap() {
    BlockHeader *first = (BlockHeader *)MY_HEAP;
    first->size = HEAP_SIZE - sizeof(BlockHeader);
    first->free = 1;
    first->prev = 0; // aucun bloc précédent
    first->next = 0; // aucun bloc suivant
}

void* my_malloc(size_t size) {
    size_t offset = 0;

    while (offset < HEAP_SIZE) {
        BlockHeader *block = (BlockHeader *)(MY_HEAP + offset);

        if (block->free && block->size >= size) {
            // Si le bloc est trop grand, on le découpe
            size_t total_needed = size + sizeof(BlockHeader);
            if (block->size > total_needed + sizeof(BlockHeader)) {
                size_t new_offset = offset + total_needed;

                BlockHeader *new_block = (BlockHeader *)(MY_HEAP + new_offset);
                new_block->size = block->size - total_needed;
                new_block->free = 1;
                new_block->prev = offset;
                new_block->next = block->next;

                // Mise à jour du bloc courant
                block->size = size;
                block->free = 0;
                block->next = new_offset;

                // Mise à jour du suivant si existant
                if (new_block->next)
                    ((BlockHeader *)(MY_HEAP + new_block->next))->prev = new_offset;
            } else {
                block->free = 0;
            }

            return (unsigned char *)block + sizeof(BlockHeader);
        }

        if (block->next == 0)
            break;
        offset = block->next;
    }

    return NULL;
}

void my_free(void *p) {
    if (!p) return;

    BlockHeader *block = (BlockHeader *)((unsigned char *)p - sizeof(BlockHeader));
    block->free = 1;

    // Fusion avec le bloc suivant s’il est libre
    if (block->next) {
        BlockHeader *next = (BlockHeader *)(MY_HEAP + block->next);
        if (next->free) {
            block->size += sizeof(BlockHeader) + next->size;
            block->next = next->next;
            if (next->next)
                ((BlockHeader *)(MY_HEAP + next->next))->prev = (unsigned char *)block - MY_HEAP;
        }
    }

    // Fusion avec le bloc précédent s’il est libre
    if (block->prev) {
        BlockHeader *prev = (BlockHeader *)(MY_HEAP + block->prev);
        if (prev->free) {
            prev->size += sizeof(BlockHeader) + block->size;
            prev->next = block->next;
            if (block->next)
                ((BlockHeader *)(MY_HEAP + block->next))->prev = block->prev;
        }
    }
}

// === prototypes de ton allocateur ===
void init_heap();
void *my_malloc(size_t size);
void my_free(void *p);

// === constants ===
#define ALLOC_COUNT 2000
#define MAX_ALLOC_SIZE 512
#define HEAP_SIZE 64000

extern unsigned char MY_HEAP[HEAP_SIZE];

// === test extrême ===
int main() {
    srand(time(NULL));
    init_heap();

    void *ptrs[ALLOC_COUNT] = {0};
    size_t total_allocated = 0;
    size_t total_freed = 0;

    for (int i = 0; i < ALLOC_COUNT * 5; i++) {
        int action = rand() % 2; // 0 = malloc, 1 = free

        if (action == 0) {
            // malloc
            int idx = rand() % ALLOC_COUNT;
            if (ptrs[idx] == NULL) {
                size_t size = rand() % MAX_ALLOC_SIZE + 1;
                void *p = my_malloc(size);
                if (p != NULL) {
                    ptrs[idx] = p;
                    total_allocated += size;
                }
            }
        } else {
            // free
            int idx = rand() % ALLOC_COUNT;
            if (ptrs[idx] != NULL) {
                my_free(ptrs[idx]);
                ptrs[idx] = NULL;
                total_freed++;
            }
        }
    }

    // libérer tout à la fin
    for (int i = 0; i < ALLOC_COUNT; i++) {
        if (ptrs[i]) {
            my_free(ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    printf("\n=== Résumé du stress test ===\n");
    printf("Allocations totales (approx.) : %zu octets\n", total_allocated);
    printf("Blocs libérés : %zu\n", total_freed);
    printf("État final : tous les blocs devraient être fusionnés.\n");

    return 0;
}

