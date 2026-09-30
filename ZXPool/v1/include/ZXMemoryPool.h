#pragma once 

#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

namespace ZX_MemoryPool
{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

/* 具体内存池的槽大小没法确定，因为每个内存池的槽大小不同(8的倍数)
   所以这个槽结构体的sizeof 不是实际的槽大小 */
struct Slot 
{
    std::atomic<Slot*> next; // 原子指针
};

class MemoryPool{

        private:

                int blockSize_;
                int slotSize_;

                Slot* firstBlock_;
                Slot* curSlot_;
                std::atomic<Slot*> freeSlotList_;
                Slot* lastSlot_;

                std::mutex mutex_Block_;

        private:

                void allocateNewBlock();
                size_t padPointer(char* p, size_t align);

                bool pushFreelist(Slot* slot);
                Slot* popFreeList();

        public: 

                MemoryPool(size_t BlockSize = 4096);
                ~MemoryPool();

                void init(size_t);

                void* allocate();
                void  deallocate(void*);

    };


    class HashBucket{

        public:
                static void init_MemoryPool();
                static  MemoryPool& getMemoryPool(int index);

                static void* hashbucket_allocateMemory(size_t size){
                        if (size <= 0)
                            return nullptr;
                        if (size > MAX_SLOT_SIZE) 
                            return operator new(size);

                        return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
                }

                 static void hashbucket_deallocateMemory(void* ptr,size_t size){
                        if (!ptr)
                            return ;
                        if (size > MAX_SLOT_SIZE){
                            operator delete(ptr);
                            return;
                        }
                        getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
                }

        template<typename T, typename... Args> 
        friend T* newElement(Args&&... args);
        
        template<typename T>
        friend void deleteElement(T* p);
    };

        template<typename T, typename... Args> 
        T* newElement(Args&&... args){
            T* p = reinterpret_cast<T*>(HashBucket::hashbucket_allocateMemory(sizeof(T)));
            if(p!=nullptr){
                new(p) T(std::forward<Args>(args)...);
            }
            return p;
        }

        template<typename T>
        void deleteElement(T* p){

            if (p){
                p->~T();
                
                HashBucket::hashbucket_deallocateMemory(reinterpret_cast<void*>(p), sizeof(T));
            }
        }

        

}