#include <stdio.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdio.h>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

typedef struct BlockHeader {
    size_t size;
    bool is_free;
    struct BlockHeader *next;
} BlockHeader;

void* global_base = NULL;

BlockHeader* request_space (BlockHeader *last, size_t size) {
    size_t total_size = size + sizeof(BlockHeader);

    void *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if(ptr == MAP_FAILED) return NULL;

    BlockHeader *block = (BlockHeader*) ptr;
    block->size = size;
    block->is_free = false;
    block->next = NULL;
    if(last) {
        last->next = block;
    }

    return block;
}
void my_free(void *ptr) {
    if(ptr == NULL) return;

    BlockHeader *h_ptr = (BlockHeader*)ptr - 1;

    h_ptr->is_free = true;
}

BlockHeader* find_free_block(BlockHeader **last, size_t size) {
    BlockHeader *current = (BlockHeader*)global_base;
    while (current) {
        if (current->is_free && current->size >= size) {
            BlockHeader *new_block = (BlockHeader*)((char*)(current + 1) + size);
        
            new_block->size = current->size - size - sizeof(BlockHeader);
            new_block->is_free = true;
            new_block->next = current->next;

            current->size = size;
            current->is_free = false;
            current->next = new_block;
            return current;
        }
        *last = current;
        current = current->next;
    }
    return NULL;
}

void* my_malloc(size_t size) {
    size = ALIGN(size);

    if (global_base == NULL) {
        BlockHeader *first = request_space(NULL, size);
        if (!first) return NULL;
        global_base = first;
        return (first + 1);
    }

    BlockHeader *last = (BlockHeader*)global_base;
    BlockHeader *found = find_free_block(&last, size);

    if (found) {
        return (found + 1);
    } else {
        BlockHeader *block = request_space(last, size);
        if (!block) return NULL;
        return (block + 1);
    }
}

int main() {
    printf("--- Starting Allocator Test ---\n");

    int *a = (int*)my_malloc(100);
    printf("Allocated A at: %p\n", a);

    my_free(a);
    printf("A is now free.\n");

    int *b = (int*)my_malloc(100);
    printf("Allocated B at: %p\n", b);

    if (a == b) {
        printf("\nSUCCESS: Memory addresses are identical. Reuse works!\n");
    } else {
        printf("\nFAILURE: New block was created instead of reusing.\n");
    }

    int *c = (int*)my_malloc(500);
    printf("Allocated C (larger size) at: %p\n", c);

    return 0;
}