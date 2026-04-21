#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h>

void* allocate(size_t size);  // tell compiler allocate exists

int main() {
    // Test small allocation
    int* a = (int*)allocate(20);
    *a = 42;
    printf("a = %d\n", *a);

    // Test another
    char* b = (char*)allocate(10);
    b[0] = 'H';
    b[1] = 'i';
    b[2] = '\0';
    printf("b = %s\n", b);

    // Test large
    int* big = (int*)allocate(2000);
    *big = 999;
    printf("big = %d\n", *big);

    return 0;
}
// DATA STRUCTURES 

typedef struct FreeBlock {
    struct FreeBlock* next;   // points to next free block
} FreeBlock;

typedef struct {
    size_t size;              // how big is this block
} BlockHeader;

// Size classes
size_t SIZE_CLASSES[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };

// Thread Cache — private per thread
typedef struct {
    FreeBlock* free_lists[8];
    int lengths[8];
} ThreadCache;

static __thread ThreadCache thread_cache = { 0 };

// Transfer Cache — shared
typedef struct {
    FreeBlock* list;
    int count;
} TransferCache;

TransferCache transfer_caches[8] = { 0 };

// HELPER 

// Find which bucket this size fits in
int get_bucket(size_t size) {
    for (int i = 0; i < 8; i++)
        if (size <= SIZE_CLASSES[i]) return i;
    return -1; // too large
}

// LAYER 3: OS 
// Called when transfer cache is empty
// mmap 4096 bytes, chop into blocks, fill transfer cache

void refill_from_os(int bucket) {
    size_t block_size = SIZE_CLASSES[bucket] + sizeof(BlockHeader);

    // Ask OS for 1 page
    char* page = mmap(NULL, 4096,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0);

    // Chop page into blocks and chain them
    int num_blocks = 4096 / block_size;
    for (int i = 0; i < num_blocks; i++) {
        FreeBlock* block = (FreeBlock*)(page + i * block_size);
        block->next = transfer_caches[bucket].list;
        transfer_caches[bucket].list = block;
    }
    transfer_caches[bucket].count = num_blocks;
}

// LAYER 2: TRANSFER CACHE
// Called when thread cache is empty
// Grab 32 blocks from transfer cache → put in thread cache

void refill_thread_cache(int bucket) {
    // Transfer cache empty? go to OS
    if (transfer_caches[bucket].count == 0)
        refill_from_os(bucket);

    // Grab 32 blocks from transfer cache
    int grab = 32;
    FreeBlock* head = transfer_caches[bucket].list;
    FreeBlock* tail = head;

    for (int i = 1; i < grab && tail->next != NULL; i++)
        tail = tail->next;

    // Cut the chain
    transfer_caches[bucket].list = tail->next;
    transfer_caches[bucket].count -= grab;
    tail->next = NULL;

    // Give to thread cache
    thread_cache.free_lists[bucket] = head;
    thread_cache.lengths[bucket] = grab;
}

// ===== MAIN ALLOCATE =====

void* allocate(size_t size) {
    if (size == 0) return NULL;

    // LARGE: go directly to OS
    if (size > 1024) {
        char* mem = mmap(NULL, sizeof(BlockHeader) + size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0);
        BlockHeader* header = (BlockHeader*)mem;
        header->size = size;
        return (void*)(header + 1);
    }

    // SMALL: find bucket
    int bucket = get_bucket(size);

    // Layer 1: thread cache empty? refill from layer 2
    if (thread_cache.free_lists[bucket] == NULL)
        refill_thread_cache(bucket);

    // Grab one block from thread cache
    FreeBlock* block = thread_cache.free_lists[bucket];
    thread_cache.free_lists[bucket] = block->next;
    thread_cache.lengths[bucket]--;

    // Write header and return pointer to user
    BlockHeader* header = (BlockHeader*)block;
    header->size = SIZE_CLASSES[bucket];
    return (void*)(header + 1);
}