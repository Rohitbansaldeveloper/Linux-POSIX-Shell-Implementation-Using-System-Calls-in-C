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

static char *heap_start = NULL;
static size_t heap_offset = 0;

/*
 * Initializes the heap arena.
 */
void mem_init(void) {
    // SYS_mmap requests memory from the kernel.
    // MAP_ANONYMOUS means it's not backed by a file (it's just RAM).
    // MAP_PRIVATE means updates are not visible to other processes.
    // We request PROT_READ | PROT_WRITE so we can read and write to this memory.
    heap_start = sys_mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (heap_start == MAP_FAILED) {
        heap_start = NULL; // Initialization failed
    }
    heap_offset = 0;
}

/*
 * A very simple "bump pointer" allocator.
 * It just moves a pointer forward in our large arena.
 * This is very fast but has a massive drawback: it never reclaims memory.
 */
void *mem_alloc(size_t size) {
    if (!heap_start) return NULL;
    
    // Align the requested size to an 8-byte boundary.
    // This is required for many architectures to safely store pointers or doubles.
    size = (size + 7) & ~7;
    
    if (heap_offset + size > HEAP_SIZE) {
        return NULL; // Out of memory in our arena
    }
    
    // Allocate the block by bumping the offset
    void *ptr = heap_start + heap_offset;
    heap_offset += size;
    
    // Zero-initialize the memory (similar to calloc) for safety
    mem_set(ptr, 0, size);
    
    return ptr;
}

/*
 * Freeing memory in a bump allocator is a no-op unless you reset the entire arena.
 * For a simple shell, this might cause memory leaks over time, but is sufficient 
 * for a basic implementation. A more robust shell would track block sizes or
 * reset the arena entirely after each command finishes executing.
 */
void mem_free(void *ptr) {
    // Intentionally left blank.
    (void)ptr; 
}
