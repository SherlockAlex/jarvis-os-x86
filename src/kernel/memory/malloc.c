#include <kernel/memory/malloc.h>
#include <kernel/kerio.h>

static MemoryManager* activate_memory_manager = 0;

static void acquire_lock(uint32_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        asm volatile ("pause");
    }
}

static void release_lock(uint32_t *lock) {
    __sync_lock_release(lock);
}

// 根据大小获取对应的类别索引
static size_t get_size_class_index(size_t size) {
    for (size_t i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (size <= size_classes[i]) {
            return i;
        }
    }
    return NUM_SIZE_CLASSES; // 表示是大块分配
}

// 从特定大小类别的空闲链表中分配内存
static void* allocate_from_size_class(MemoryManager* manager, size_t size, size_t class_idx) {
    if (class_idx >= NUM_SIZE_CLASSES) {
        // 大块分配，需要获取大块锁
        acquire_lock(&manager->large_lock);
        
        MemoryChunk *best_fit = 0;
        MemoryChunk *prev = 0;
        MemoryChunk *current = manager->first;
        
        while (current != 0) {
            if (!current->allocated && current->size >= size) {
                if (best_fit == 0 || current->size < best_fit->size) {
                    best_fit = current;
                }
            }
            current = current->next;
        }
        
        if (best_fit == 0) {
            release_lock(&manager->large_lock);
            kernel_printf("allocate_from_size_class: no memory for size %d\n", size);
            return 0;
        }
        
        // 从链表中移除找到的块
        if (best_fit->prev) {
            best_fit->prev->next = best_fit->next;
        } else {
            manager->first = best_fit->next;
        }
        
        if (best_fit->next) {
            best_fit->next->prev = best_fit->prev;
        }
        
        // 分割块（如果需要）
        if (best_fit->size >= size + sizeof(MemoryChunk) + MIN_ALLOC_SIZE) {
            MemoryChunk *temp = (MemoryChunk *)((size_t)best_fit + sizeof(MemoryChunk) + size);
            temp->allocated = 0;
            temp->size = best_fit->size - size - sizeof(MemoryChunk);
            temp->size_class = NUM_SIZE_CLASSES;
            temp->prev = 0;
            temp->next = manager->first;
            
            if (manager->first) {
                manager->first->prev = temp;
            }
            manager->first = temp;
            
            best_fit->size = size;
        }
        
        best_fit->allocated = 1;
        best_fit->size_class = NUM_SIZE_CLASSES;
        best_fit->prev = 0;
        best_fit->next = 0;
        
        release_lock(&manager->large_lock);
        return (void *)((size_t)best_fit + sizeof(MemoryChunk));
    }

    acquire_lock(&manager->class_locks[class_idx]);
    
    if (manager->free_lists[class_idx] == 0) {
        release_lock(&manager->class_locks[class_idx]);
        
        // 获取大块锁进行分配
        acquire_lock(&manager->large_lock);
        refill_size_class(manager, class_idx);
        release_lock(&manager->large_lock);
        
        // 重新获取类别锁
        acquire_lock(&manager->class_locks[class_idx]);
        
        if (manager->free_lists[class_idx] == 0) {
            release_lock(&manager->class_locks[class_idx]);
            return 0; // 内存不足
        }
    }
    
    MemoryChunk *chunk = manager->free_lists[class_idx];
    manager->free_lists[class_idx] = chunk->next;
    
    chunk->allocated = 1;
    
    release_lock(&manager->class_locks[class_idx]);
    return (void *)((size_t)chunk + sizeof(MemoryChunk));
}


static void refill_size_class(MemoryManager* manager, size_t class_idx) {
    size_t block_size = size_classes[class_idx];
    size_t total_size = 4 * 1024; // 一次分配4KB
    
    // 从大块内存中分配一块
    MemoryChunk *big_chunk = 0;
    MemoryChunk *prev_chunk = 0;
    for (MemoryChunk *chunk = manager->first; chunk != 0 && big_chunk == 0; chunk = chunk->next) {
        if (!chunk->allocated && chunk->size >= total_size) {
            big_chunk = chunk;
            break;
        }
        prev_chunk = chunk;
    }
    
    if (big_chunk == 0) {
        kernel_printf("refill_size_class: no big chunk found for class %d\n", class_idx);
        return; // 没有足够的大块内存
    }
    
    // 从链表中移除大块
    if (prev_chunk) {
        prev_chunk->next = big_chunk->next;
        if (big_chunk->next) {
            big_chunk->next->prev = prev_chunk;
        }
    } else {
        manager->first = big_chunk->next;
        if (big_chunk->next) {
            big_chunk->next->prev = 0;
        }
    }
    
    // 计算可以分割出多少个小块
    size_t chunk_count = total_size / (block_size + sizeof(MemoryChunk));
    if (chunk_count == 0) {
        chunk_count = 1; // 至少一个块
    }
    
    // 分割大块为多个小块并添加到空闲链表
    char *current_pos = (char *)big_chunk;
    for (size_t i = 0; i < chunk_count; i++) {
        MemoryChunk *new_chunk = (MemoryChunk *)current_pos;
        new_chunk->size = block_size;
        new_chunk->allocated = 0;
        new_chunk->size_class = class_idx;
        new_chunk->prev = 0;
        
        // 添加到对应类别的空闲链表
        new_chunk->next = manager->free_lists[class_idx];
        if (manager->free_lists[class_idx]) {
            manager->free_lists[class_idx]->prev = new_chunk;
        }
        manager->free_lists[class_idx] = new_chunk;
        
        current_pos += sizeof(MemoryChunk) + block_size;
    }
    
    // 如果有剩余内存，将其添加回大块链表
    size_t remaining_size = big_chunk->size - total_size;
    if (remaining_size > sizeof(MemoryChunk) + MIN_ALLOC_SIZE) {
        MemoryChunk *remaining_chunk = (MemoryChunk *)((char *)big_chunk + total_size);
        remaining_chunk->size = remaining_size - sizeof(MemoryChunk);
        remaining_chunk->allocated = 0;
        remaining_chunk->size_class = NUM_SIZE_CLASSES;
        remaining_chunk->prev = 0;
        remaining_chunk->next = manager->first;
        
        if (manager->first) {
            manager->first->prev = remaining_chunk;
        }
        manager->first = remaining_chunk;
    }
}

void on_init_memory_manager(MemoryManager* manager, size_t start, size_t size) {
    activate_memory_manager = manager;
    
    // 初始化所有空闲链表和锁
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        manager->free_lists[i] = 0;
        manager->class_locks[i] = 0;
    }
    manager->large_lock = 0;

    if (size < sizeof(MemoryChunk)) {
        manager->first = 0;
    } else {
        manager->first = (MemoryChunk *)start;
        manager->first->allocated = 0;
        manager->first->prev = 0;
        manager->first->next = 0;
        manager->first->size = size - sizeof(MemoryChunk);
        manager->first->size_class = NUM_SIZE_CLASSES; // 标记为大块
    }

    kernel_printf("init memory manager success!\nthe first memory chunk address:%x\n", manager->first);
}

void print_memory_status() {
    if (activate_memory_manager == 0) {
        kernel_printf("Memory manager not initialized\n");
        return;
    }
    
    // 获取大块锁以确保线程安全
    acquire_lock(&activate_memory_manager->large_lock);
    
    kernel_printf("Memory status:\n");
    kernel_printf("Large blocks:\n");
    
    int large_count = 0;
    size_t large_free = 0;
    for (MemoryChunk *chunk = activate_memory_manager->first; chunk != 0; chunk = chunk->next) {
        kernel_printf("  Chunk at %x: size=%d, allocated=%d\n", 
                     chunk, chunk->size, chunk->allocated);
        large_count++;
        if (!chunk->allocated) {
            large_free += chunk->size;
        }
    }
    
    kernel_printf("Total large blocks: %d, free memory: %d bytes\n", large_count, large_free);
    
    // 释放大块锁
    release_lock(&activate_memory_manager->large_lock);
    
    kernel_printf("Size class free lists:\n");
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        // 获取每个类别的锁
        acquire_lock(&activate_memory_manager->class_locks[i]);
        
        int count = 0;
        for (MemoryChunk *chunk = activate_memory_manager->free_lists[i]; chunk != 0; chunk = chunk->next) {
            count++;
        }
        kernel_printf("  Class %d (%d bytes): %d free blocks\n", i, size_classes[i], count);
        
        // 释放类别锁
        release_lock(&activate_memory_manager->class_locks[i]);
    }
}

void *malloc(size_t size) {
    if (activate_memory_manager == 0) {
        return 0;
    }

    // 进行内存对齐，确保是最小分配单位的整数倍
    size = (size + MIN_ALLOC_SIZE - 1) & ~(MIN_ALLOC_SIZE - 1);
    
    // 获取大小类别索引
    size_t class_idx = get_size_class_index(size);
    
    // 从对应类别分配内存
    void* result = allocate_from_size_class(activate_memory_manager, size, class_idx);
    
    if (result == 0) {
        kernel_printf("malloc: allocation failed for size %d\n", size);
        // 打印内存状态用于调试
        print_memory_status();
    }
    return result;
}

void free(void *ptr) {
    if (activate_memory_manager == 0 || ptr == 0) {
        return;
    }
    
    MemoryChunk *chunk = (MemoryChunk *)((size_t)ptr - sizeof(MemoryChunk));
    
    // 如果是小块分配，获取对应类别的锁
    if (chunk->size_class < NUM_SIZE_CLASSES) {
        acquire_lock(&activate_memory_manager->class_locks[chunk->size_class]);
        
        chunk->allocated = 0;
        chunk->next = activate_memory_manager->free_lists[chunk->size_class];
        if (chunk->next) {
            chunk->next->prev = chunk;
        }
        activate_memory_manager->free_lists[chunk->size_class] = chunk;
        
        release_lock(&activate_memory_manager->class_locks[chunk->size_class]);
        return;
    }
    
    // 大块分配，获取大块锁
    acquire_lock(&activate_memory_manager->large_lock);
    
    chunk->allocated = 0;

    // 向前合并所有空闲块
    while (chunk->prev != 0 && !chunk->prev->allocated) {
        MemoryChunk *prev = chunk->prev;
        prev->next = chunk->next;
        prev->size += chunk->size + sizeof(MemoryChunk);
        if (chunk->next != 0) {
            chunk->next->prev = prev;
        }
        chunk = prev;
    }

    // 向后合并所有空闲块
    while (chunk->next != 0 && !chunk->next->allocated) {
        MemoryChunk *next = chunk->next;
        chunk->next = next->next;
        chunk->size += next->size + sizeof(MemoryChunk);
        if (next->next != 0) {
            next->next->prev = chunk;
        }
    }

    release_lock(&activate_memory_manager->large_lock);
}