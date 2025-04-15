// memory_manager.c
#include "memory_manager.h"

// block_info prints information of the block
void block_info(struct MemBlock *mblock)
{
    // Lock the mutex   
    pthread_mutex_lock(&mem_lock);

    printf("\nMemBlock: %p\n", mblock);
    printf("Ptr: %p\n", mblock->ptr);
    printf("size: %zu\n", mblock->size);
    printf("Next: %p\n", mblock->next);

    // Unlock the mutex
    pthread_mutex_unlock(&mem_lock);
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

    // Allocate memory for the block
    struct MemBlock* block = (struct MemBlock*)malloc(sizeof(struct MemBlock));
    if (!block) 
    {
        return NULL;
    }

    // Initialize the block
    block->ptr = ptr;
    block->size = size;
    block->next = next;

    return block;
}

// block_find finds the block and returns ptr of the previous block
struct MemBlock* block_find(void* block)
{
    // Defind the block
    struct MemBlock* prevBlock = &MemPool;

    // Find the block
    while (prevBlock->next != NULL)
    {
        if (prevBlock->next->ptr == block)
        {
            return prevBlock;
        }
    
        // Continue to the next block
        prevBlock = prevBlock->next;
    }

    // fprintf(stderr, "block_find failed, can not find block %p in the memory pool.\n", block);
    return prevBlock;
};

// mem_init initializes memory pool
void mem_init(size_t size)
{
    // Lock the mutex
    pthread_mutex_lock(&mem_lock);
    // Allocate space in the memory
    void* ptr = malloc(size);
    if (!ptr) 
    {
        fprintf(stderr, "mem_init failed, can not allocate memory.\n");
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    // Initialize MemPool
    MemPool.ptr = ptr;
    MemPool.size = size;
    MemPool.next = NULL;

    // Unlock the mutex
    pthread_mutex_unlock(&mem_lock);
}

// mem_alloc allocates space in the memory pool
void* mem_alloc(size_t size)
{
    // Lock the mutex
    pthread_mutex_lock(&mem_lock);

    // Check if size of MemBlock is greater than 0
    if (size <= 0)
    {
        // fprintf(stderr, "mem_alloc error: Too small, block size is %zu\n", size);
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    // Check if enough space in the Memory pool
    if (size > MemPool.size)
    {
        // fprintf(stderr, "mem_alloc error: Too large, Memory pool size is %zu.\n", MemPool.size);
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    void* result = NULL;

    // Check if Memory pool is empty
    if (MemPool.next == NULL)
    {
        MemPool.next = block_init(MemPool.ptr, size, NULL);
        if (MemPool.next) 
        {
            result = MemPool.next->ptr;
        }
        pthread_mutex_unlock(&mem_lock);
        return result;
    }

    // Check if it's space before the first block
    if (MemPool.next->ptr != MemPool.ptr && (MemPool.ptr + size) <= MemPool.next->ptr) 
    {
        struct MemBlock *new_block = block_init(MemPool.ptr, size, MemPool.next);
        if (new_block) {
            MemPool.next = new_block;
            result = new_block->ptr;
        }
        pthread_mutex_unlock(&mem_lock);
        return result;
    }

    // Check for space between blocks
    struct MemBlock* current = MemPool.next;
    while (current->next != NULL) {
        void* gap_start = current->ptr + current->size;
        void* gap_end = current->next->ptr;
        
        if ((gap_start + size) <= gap_end) {
            struct MemBlock *new_block = block_init(gap_start, size, current->next);
            if (new_block) {
                current->next = new_block;
                result = new_block->ptr;
            }
            pthread_mutex_unlock(&mem_lock);
            return result;
        }
        current = current->next;
    }

    // Check if it's enough space at the end
    void* end_ptr = current->ptr + current->size;
    if ((end_ptr + size) <= (MemPool.ptr + MemPool.size)) {
        struct MemBlock *new_block = block_init(end_ptr, size, NULL);
        if (new_block) {
            current->next = new_block;
            result = new_block->ptr;
        }
    }

    pthread_mutex_unlock(&mem_lock);
    return result;
}

// Free the allocated space in the memory pool
void mem_free(void* block)
{
    // Lock the mutex
    pthread_mutex_lock(&mem_lock);

    // Check if block ptr is null
    if (!block) 
    {
        // fprintf(stderr, "mem_free failed, block ptr is null.\n");
        pthread_mutex_unlock(&mem_lock);
        return;
    }

    // Check if MemPool is empty
    if (!MemPool.next) 
    {
        // fprintf(stderr, "mem_free failed, MemPool is empty.\n");
        pthread_mutex_unlock(&mem_lock);
        return;
    }
    
    // Defind the previous block to the block
    struct MemBlock* prevBlock = block_find(block);

    // Check if block exists
    if (!prevBlock->next) return;
    
    // Remove the block
    struct MemBlock *temp = prevBlock->next->next;
    free(prevBlock->next);
    if (!prevBlock->next) 
    {
        fprintf(stderr, "mem_free failed, free the block %p failed.\n", block);
    }
    prevBlock->next = temp;

    // Unlock the mutex
    pthread_mutex_unlock(&mem_lock);
}

// mem_resize resizes the block size and returns the new ptr
void* mem_resize(void* block, size_t size)
{
    if (!block || size == 0) 
    {
        return NULL;
    }

    // Lock the mutex
    pthread_mutex_lock(&mem_lock);

    // Find the block
    struct MemBlock* prevBlock = block_find(block);
    if (!prevBlock || !prevBlock->next) 
    {
        pthread_mutex_unlock(&mem_lock);
        return NULL;
    }

    struct MemBlock* current = prevBlock->next;
    size_t old_size = current->size;

    // If new size is smaller, just update the size
    if (size <= old_size) {
        current->size = size;
        pthread_mutex_unlock(&mem_lock);
        return block;
    }

    // Try to expand in place if possible
    if (current->next && 
        (current->ptr + size) <= current->next->ptr) {
        current->size = size;
        pthread_mutex_unlock(&mem_lock);
        return block;
    }

    // Need to allocate new block and copy data
    pthread_mutex_unlock(&mem_lock);
    void* new_block = mem_alloc(size);
    if (!new_block) 
    {
        return NULL;
    }

    // Copy the data
    memcpy(new_block, block, old_size);
    
    // Free the old block
    mem_free(block);

    return new_block;
}

// mem_deinit frees all memory of the pool
void mem_deinit()
{
    // Lock the mutex
    pthread_mutex_lock(&mem_lock);

    // Free all mblock
    struct MemBlock* mblock = MemPool.next;
    while(mblock != NULL)
    {
        struct MemBlock* next = mblock->next;
        free(mblock->ptr);
        mblock = next;
    }

    // Free the pool
    free(MemPool.ptr);

    // Unlock the mutex
    pthread_mutex_unlock(&mem_lock);
}
