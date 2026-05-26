/*
 * memory.c
 * 
 * A minimalistic custom memory allocator. 
 * Since we don't have glibc's malloc(), we must request raw memory pages
 * from the Linux kernel directly using the mmap() system call.
 */

#include "memory.h"
#include "string_utils.h"

#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)

// We allocate a single, large 16 MB chunk of memory upfront (an arena).
#define HEAP_SIZE (1024 * 1024 * 16)
#define PERMANENT_SIZE (1024 * 1024 * 8) // First 8MB for malloc

typedef struct MemBlock {
    size_t size;
    int is_free;
    struct MemBlock *next;
} MemBlock;

static char *heap_start = NULL;
static MemBlock *head = NULL;
static size_t temp_offset = HEAP_SIZE;

/*
 * Initializes the heap arena.
 */
void mem_init(void) {
    heap_start = sys_mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (heap_start != MAP_FAILED) {
        head = (MemBlock *)heap_start;
        head->size = PERMANENT_SIZE - sizeof(MemBlock);
        head->is_free = 1;
        head->next = NULL;
        
        temp_offset = HEAP_SIZE;
    } else {
        heap_start = NULL;
    }
}

/*
 * A block-based memory allocator replacing malloc.
 * It searches for a free block, splits it if necessary, and returns the pointer.
 */
void *mem_alloc(size_t size) {
    if (!head || size == 0) return NULL;
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    MemBlock *curr = head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            // Split the block if there is enough excess space
            if (curr->size >= size + sizeof(MemBlock) + 8) {
                MemBlock *new_block = (MemBlock *)((char *)curr + sizeof(MemBlock) + size);
                new_block->size = curr->size - size - sizeof(MemBlock);
                new_block->is_free = 1;
                new_block->next = curr->next;
                
                curr->size = size;
                curr->is_free = 0;
                curr->next = new_block;
            } else {
                curr->is_free = 0;
            }
            void *ptr = (void *)(curr + 1);
            mem_set(ptr, 0, curr->size);
            return ptr;
        }
        curr = curr->next;
    }
    return NULL; // Out of memory
}

/*
 * Allocate memory for temporary parsing and AST construction.
 * Grows backwards from the top of the arena (fast bump allocator).
 */
void *mem_alloc_temp(size_t size) {
    if (!heap_start) return NULL;
    
    size = (size + 7) & ~7;
    
    // Ensure it doesn't crash into permanent memory limit (8MB)
    if (temp_offset - size < PERMANENT_SIZE) {
        return NULL; // Out of memory
    }
    
    temp_offset -= size;
    void *ptr = heap_start + temp_offset;
    mem_set(ptr, 0, size);
    return ptr;
}

void mem_reset_temp(void) {
    temp_offset = HEAP_SIZE;
}

/*
 * Reclaims memory by marking block as free and coalescing adjacent free blocks.
 */
void mem_free(void *ptr) {
    if (!ptr) return;
    
    MemBlock *block = (MemBlock *)ptr - 1;
    block->is_free = 1;
    
    // Coalesce adjacent free blocks
    MemBlock *curr = head;
    while (curr) {
        if (curr->is_free && curr->next && curr->next->is_free) {
            curr->size += sizeof(MemBlock) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}
