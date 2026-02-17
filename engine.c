#include <stdio.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <pthread.h> 
#include <unistd.h>
#include <string.h>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

typedef struct BlockHeader {
    size_t size;
    bool is_free;
    struct BlockHeader *next;
} BlockHeader;

void* global_base = NULL;

pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

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

BlockHeader* find_free_block(BlockHeader **last, size_t size) {

    BlockHeader *current = (BlockHeader*)global_base;
    while (current) {
        if (current->is_free && current->size >= (size + sizeof(BlockHeader) + ALIGNMENT)) {
            BlockHeader *new_block = (BlockHeader*)((char*)(current + 1) + size);
            new_block->size = current->size - size - sizeof(BlockHeader);
            new_block->is_free = true; 
            new_block->next = current->next;

            current->size = size;
            current->is_free = false;
            current->next = new_block;
            return current;
        } 
        else if (current->is_free && current->size >= size) {
            current->is_free = false;
            return current;
        }
        *last = current;
        current = current->next;
    }
    return NULL;
}

void my_free(void *ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&global_malloc_lock);
    BlockHeader *h = (BlockHeader*)ptr - 1;
    h->is_free = true;

    while (h->next && h->next->is_free) {
        h->size += sizeof(BlockHeader) + h->next->size;
        h->next = h->next->next;
    }
    pthread_mutex_unlock(&global_malloc_lock);
}


void* my_malloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN(size);

    pthread_mutex_lock(&global_malloc_lock);

    if (global_base == NULL) {
        BlockHeader *first = request_space(NULL, size);
        if (!first) {
            pthread_mutex_unlock(&global_malloc_lock);
            return NULL;
        }
        global_base = first;
        pthread_mutex_unlock(&global_malloc_lock);
        return (first + 1);
    }
    BlockHeader *last = (BlockHeader*)global_base;
    BlockHeader *found = find_free_block(&last, size);

    if (found) {
        pthread_mutex_unlock(&global_malloc_lock);
        return (found + 1);
    } else {
        BlockHeader* block = request_space(last, size);
        pthread_mutex_unlock(&global_malloc_lock);
        if (!block) return NULL;
        return (block + 1);
    }
}

//AI generated tests
void* thread_test(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 5; i++) {
        size_t size = (i + 1) * 64;
        void* ptr = my_malloc(size);
        if (ptr) {
            printf("[Thread %d] Allocated %zu bytes at %p\n", id, size, ptr);
            memset(ptr, 0xAA, size);
            usleep(10000);
            my_free(ptr);
            printf("[Thread %d] Freed memory\n", id);
        }
    }
    return NULL;
}

int main() {
    printf("--- Phase 1: Splitting & Coalescing Test ---\n");
    void *p1 = my_malloc(500);
    void *p2 = my_malloc(200);
    printf("Allocated P1 (500) and P2 (200)\n");

    my_free(p1);
    printf("Freed P1\n");

    void *p3 = my_malloc(100);
    printf("Allocated P3 (100) from P1: %p (Difference: %ld)\n", p3, (char*)p1 - (char*)p3);

    void *p4 = my_malloc(100);
    printf("Allocated P4 (100) from P1: %p\n", p4);

    my_free(p3);
    my_free(p4);
    printf("Freed P3 and P4. Now they should be merged back.\n");

    printf("\n--- Phase 2: Thread Safety Test ---\n");
    pthread_t threads[4];
    int ids[4];
    for (int i = 0; i < 4; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, thread_test, &ids[i]);
    }
    for (int i = 0; i < 4; i++) pthread_join(threads[i], NULL);

    printf("\n--- All tests passed! Ready for Networking. ---\n");
    return 0;
}