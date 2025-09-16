#ifndef OS_KERNEL_MEMORY_MALLOC
#define OS_KERNEL_MEMORY_MALLOC

#include <stdtype.h>

#define MIN_ALLOC_SIZE 16
#define NUM_SIZE_CLASSES 7  // 增加大小类别数量

// 动态内存分配

// 定义大小类别
static const size_t size_classes[NUM_SIZE_CLASSES] = {
    16, 32, 64, 128, 256, 512, 1024
};

typedef enum Bool Bool;

typedef struct MemoryChunk
{
    struct MemoryChunk* next;
    struct MemoryChunk* prev;
    uint8_t allocated;          // 被分配是返回1,回收后返回0
    size_t size;
    uint8_t size_class;         // 新增：记录大小类别索引，用于快速释放
} MemoryChunk;

// 为每个大小类别的空闲链表单独加锁
typedef struct MemoryManager {
    MemoryChunk* first;
    MemoryChunk* free_lists[NUM_SIZE_CLASSES];
    uint32_t large_lock;  // 大块内存的锁
    uint32_t class_locks[NUM_SIZE_CLASSES];  // 每个大小类别的锁
} MemoryManager;


extern void on_init_memory_manager(MemoryManager*, size_t first, size_t size);
extern void* malloc(size_t size);
extern void free(void* ptr);

// 辅助函数声明
static size_t get_size_class_index(size_t size);
static void* allocate_from_size_class(MemoryManager* manager, size_t size, size_t class_idx);
static void refill_size_class(MemoryManager* manager, size_t class_idx);

// 同时根据时间局部性原理和空间局部性原理，需要实现虚拟内存

#endif