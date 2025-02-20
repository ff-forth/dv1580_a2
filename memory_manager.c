#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "memory_manager.h"

// block_info prints information of the block
void block_info(struct MemBlock *mblock)
{
    printf("\nMemBlock: %p\n", mblock);
    printf("Ptr: %p\n", mblock->ptr);
    printf("size: %zu\n", mblock->size);
    printf("Next: %p\n", mblock->next);
}

// pool_info prints informations of all block in the pool
void pool_info()
{
    struct MemBlock* mblock = &MemPool;
    
    while(mblock != NULL)
    {
        block_info(mblock);
        mblock = mblock->next;
    }
    
}

// block_init creates a MemBlock in in the memory pool
// and returns ptr of the created block
struct MemBlock* block_init(void* ptr, size_t size, void* next)
{
    void* blockptr = malloc(sizeof(struct MemBlock));
    struct MemBlock* block = (struct MemBlock*)blockptr;
    
    block->ptr = ptr;
    block->size = size;
    block->next = next;
    pthread_mutex_init(&block->mutex, NULL);  // Initialize mutex for new block

    return block;
}

// block_find finds the block and returns ptr of the searched block
struct MemBlock* block_find(void* block)
{
    // Define the block
    struct MemBlock* mblock = &MemPool;

    // Find the block that points to our target
    while (mblock->next != NULL)
    {
        if (mblock->next->ptr == block)
        {
            return mblock;
        }
        mblock = mblock->next;
    }

    return NULL;
}

// mem_init initializes memory pool
void mem_init(size_t size)
{
    // Allocate space in the memory
    void* ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "mem_init error: Failed to allocate memory pool of size %zu\n", size);
        return;
    }

    // Initialize MemPool
    MemPool.ptr = ptr;
    MemPool.size = size;
    MemPool.next = NULL;
    
    // Initialize mutex
    pthread_mutex_init(&MemPool.mutex, NULL);
}

// mem_alloc allocates space in the memory pool
void* mem_alloc(size_t size)
{
    // Handle minimum size allocation
    if (size == 0) {
        size = sizeof(void*);
    }

    pthread_mutex_lock(&MemPool.mutex);

    // Validate memory pool
    if (MemPool.ptr == NULL) {
        pthread_mutex_unlock(&MemPool.mutex);
        return NULL;
    }

    // Check available space
    if (size > MemPool.size) {
        pthread_mutex_unlock(&MemPool.mutex);
        return NULL;
    }

    // First allocation
    if (MemPool.next == NULL) {
        struct MemBlock* new_block = block_init(MemPool.ptr, size, NULL);
        if (new_block == NULL) {
            pthread_mutex_unlock(&MemPool.mutex);
            return NULL;
        }
        MemPool.next = new_block;
        pthread_mutex_unlock(&MemPool.mutex);
        return new_block->ptr;
    }

    // Try to allocate at the start
    if (((char*)MemPool.next->ptr - (char*)MemPool.ptr) >= size) {
        struct MemBlock* new_block = block_init(MemPool.ptr, size, MemPool.next);
        if (new_block == NULL) {
            pthread_mutex_unlock(&MemPool.mutex);
            return NULL;
        }
        MemPool.next = new_block;
        pthread_mutex_unlock(&MemPool.mutex);
        return new_block->ptr;
    }

    // Find a gap between blocks
    struct MemBlock* current = MemPool.next;
    while (current->next != NULL) {
        size_t gap = (char*)current->next->ptr - ((char*)current->ptr + current->size);
        if (gap >= size) {
            struct MemBlock* new_block = block_init((char*)current->ptr + current->size, 
                                                  size, current->next);
            if (new_block == NULL) {
                pthread_mutex_unlock(&MemPool.mutex);
                return NULL;
            }
            current->next = new_block;
            pthread_mutex_unlock(&MemPool.mutex);
            return new_block->ptr;
        }
        current = current->next;
    }

    // Try to allocate at the end
    size_t remaining = MemPool.size - ((char*)current->ptr + current->size - (char*)MemPool.ptr);
    if (remaining >= size) {
        struct MemBlock* new_block = block_init((char*)current->ptr + current->size, 
                                              size, NULL);
        if (new_block == NULL) {
            pthread_mutex_unlock(&MemPool.mutex);
            return NULL;
        }
        current->next = new_block;
        pthread_mutex_unlock(&MemPool.mutex);
        return new_block->ptr;
    }

    pthread_mutex_unlock(&MemPool.mutex);
    return NULL;
}

// Free the allocated space in the memory pool
void mem_free(void* block)
{
    // Check if block ptr is null
    if (block == NULL)
    {
        fprintf(stderr, "mem_free error: block ptr is null.\n");
        return;
    }

    pthread_mutex_lock(&MemPool.mutex);

    // Check if memory pool is empty
    if (MemPool.next == NULL)
    {
        pthread_mutex_unlock(&MemPool.mutex);
        fprintf(stderr, "mem_free error: Memory pool is empty.\n");
        return;
    }
    
    // Find the block to free
    struct MemBlock* prev = block_find(block);
    if (prev == NULL)
    {
        // Special case: check if it's the MemPool itself
        if (MemPool.ptr == block)
        {
            pthread_mutex_unlock(&MemPool.mutex);
            return;  // Don't free MemPool
        }
        pthread_mutex_unlock(&MemPool.mutex);
        return;  // Silently ignore non-existent blocks
    }

    // Get the block to free
    struct MemBlock* to_free = prev->next;
    prev->next = to_free->next;

    // Destroy mutex and free the block
    pthread_mutex_destroy(&to_free->mutex);
    free(to_free);
    
    pthread_mutex_unlock(&MemPool.mutex);
}

// mem_resize resizes the block size and returns the new ptr
void* mem_resize(void* block, size_t size)
{
    pthread_mutex_lock(&MemPool.mutex);
    
    // Find the block
    struct MemBlock* mblock = block_find(block);
    
    // Store block info
    mblock = mblock->next;
    void *mstart = mblock->ptr;
    size_t msize = mblock->size;

    // Check if new size is smaller
    if (size <= msize)
    {
        mblock->size = size;
        pthread_mutex_unlock(&MemPool.mutex);
        return block;
    }

    pthread_mutex_unlock(&MemPool.mutex);
    
    // Free old block and allocate new one
    mem_free(block);
    void* new_block = mem_alloc(size);
    
    if (new_block != NULL) {
        memcpy(new_block, mstart, msize);
    } else {
        fprintf(stderr, "mem_resize failed, can not find enough space.\n");
        new_block = mem_alloc(msize);
        if (new_block != NULL) {
            memcpy(new_block, mstart, msize);
        }
    }
    
    return new_block;
}

// mem_deinit frees all memory of the pool
void mem_deinit()
{
    if (!MemPool.ptr) {
        return;  // Already deinitialized
    }

    pthread_mutex_lock(&MemPool.mutex);
    
    // Free all blocks
    while(MemPool.next != NULL)
    {
        struct MemBlock *to_free = MemPool.next;
        MemPool.next = to_free->next;
        
        // Destroy mutex before freeing block
        pthread_mutex_destroy(&to_free->mutex);
        free(to_free);
    }

    // Free the pool memory
    void* pool_ptr = MemPool.ptr;
    MemPool.ptr = NULL;  // Mark as deinitialized
    free(pool_ptr);
    
    // Destroy and unlock MemPool mutex
    pthread_mutex_unlock(&MemPool.mutex);
    pthread_mutex_destroy(&MemPool.mutex);
}
