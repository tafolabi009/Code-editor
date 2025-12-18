#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>

namespace memory {

/**
 * @brief Memory block header for tracking allocations
 */
struct BlockHeader {
    size_t size;
    size_t alignment;
    BlockHeader* next;  // For free list
    bool inUse;
};

/**
 * @brief Fixed-size block pool for common allocation sizes
 */
class BlockPool {
public:
    BlockPool(size_t blockSize, size_t initialBlocks = 64);
    ~BlockPool();
    
    void* allocate();
    void deallocate(void* ptr);
    
    size_t getBlockSize() const { return m_blockSize; }
    size_t getTotalBlocks() const { return m_totalBlocks; }
    size_t getFreeBlocks() const { return m_freeBlocks; }
    size_t getUsedBlocks() const { return m_totalBlocks - m_freeBlocks; }
    
private:
    size_t m_blockSize;
    size_t m_totalBlocks;
    size_t m_freeBlocks;
    uint8_t* m_memory;
    BlockHeader* m_freeList;
    std::mutex m_mutex;
    
    void grow(size_t additionalBlocks);
};

/**
 * @brief Arena allocator for bulk allocations
 * 
 * Fast bump-pointer allocation with no individual deallocation.
 * Memory is freed all at once when the arena is reset or destroyed.
 */
class ArenaAllocator {
public:
    static constexpr size_t DEFAULT_BLOCK_SIZE = 64 * 1024;  // 64KB
    
    explicit ArenaAllocator(size_t blockSize = DEFAULT_BLOCK_SIZE);
    ~ArenaAllocator();
    
    // Non-copyable, moveable
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&& other) noexcept;
    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;
    
    // Allocation
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));
    
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate(sizeof(T), alignof(T));
        return new(ptr) T(std::forward<Args>(args)...);
    }
    
    // Bulk deallocation
    void reset();
    void clear() { reset(); }
    
    // Statistics
    size_t getTotalAllocated() const { return m_totalAllocated; }
    size_t getTotalCapacity() const { return m_blocks.size() * m_blockSize; }
    size_t getBlockCount() const { return m_blocks.size(); }
    
private:
    struct Block {
        uint8_t* memory;
        size_t size;
    };
    
    std::vector<Block> m_blocks;
    size_t m_blockSize;
    size_t m_currentBlock = 0;
    size_t m_offset = 0;
    size_t m_totalAllocated = 0;
    
    void allocateNewBlock();
    static size_t alignUp(size_t value, size_t alignment);
};

/**
 * @brief Custom allocator for text buffers
 * 
 * Optimized for:
 * - Frequent small allocations (line data)
 * - Cache-line aligned data for SIMD
 * - Low fragmentation
 */
class TextAllocator {
public:
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t SMALL_BLOCK_SIZE = 4 * 1024;    // 4KB
    static constexpr size_t MEDIUM_BLOCK_SIZE = 64 * 1024;  // 64KB
    static constexpr size_t LARGE_BLOCK_SIZE = 1024 * 1024; // 1MB
    
    TextAllocator();
    ~TextAllocator();
    
    // Allocation
    void* allocate(size_t size);
    void* allocateAligned(size_t size, size_t alignment);
    void deallocate(void* ptr, size_t size);
    
    // SIMD-aligned allocation (64-byte alignment for AVX-512)
    void* allocateSIMD(size_t size);
    void deallocateSIMD(void* ptr, size_t size);
    
    // String allocation
    char* allocateString(size_t length);
    void deallocateString(char* str, size_t length);
    
    // Statistics
    size_t getTotalAllocated() const { return m_totalAllocated; }
    size_t getPeakAllocated() const { return m_peakAllocated; }
    size_t getAllocationCount() const { return m_allocationCount; }
    
    // Reset and defragment
    void reset();
    void defragment();
    
    // Singleton instance
    static TextAllocator& instance();
    
private:
    BlockPool m_smallPool{SMALL_BLOCK_SIZE};
    BlockPool m_mediumPool{MEDIUM_BLOCK_SIZE};
    BlockPool m_largePool{LARGE_BLOCK_SIZE};
    ArenaAllocator m_arena;
    
    size_t m_totalAllocated = 0;
    size_t m_peakAllocated = 0;
    size_t m_allocationCount = 0;
    
    std::mutex m_mutex;
    
    BlockPool* selectPool(size_t size);
};

// ====================
// ASM-optimized functions (extern "C" for linkage with assembly)
// ====================

#ifdef HAS_ASM_OPTIMIZATIONS
extern "C" {
    /**
     * @brief Fast memory copy with SIMD
     */
    void simd_memcpy(void* dst, const void* src, size_t size);
    
    /**
     * @brief Fast memory set with SIMD
     */
    void simd_memset(void* dst, int value, size_t size);
    
    /**
     * @brief Fast memory compare with SIMD
     */
    int simd_memcmp(const void* a, const void* b, size_t size);
    
    /**
     * @brief Aligned allocation from custom allocator
     */
    void* asm_aligned_alloc(size_t size, size_t alignment);
    
    /**
     * @brief Free aligned allocation
     */
    void asm_aligned_free(void* ptr);
}
#endif

} // namespace memory
