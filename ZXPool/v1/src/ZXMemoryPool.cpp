#include "../include/ZXMemoryPool.h"

namespace ZX_MemoryPool{


    MemoryPool::MemoryPool(size_t blocksize)
            : blockSize_(blocksize),
              slotSize_(0),
              firstBlock_(nullptr),
              curSlot_(nullptr),
              lastSlot_(nullptr),
              freeSlotList_(nullptr)
            {}
    
    MemoryPool::~MemoryPool(){
            Slot* cur = firstBlock_;
            while (cur)
            {
                Slot* next = cur->next;
                operator delete(reinterpret_cast<void*> (cur));
                cur = next;
            }
            
    }
            
    void MemoryPool::init(size_t size)
    {
        assert(size > 0);
        slotSize_ = size;
        firstBlock_ = nullptr;
        curSlot_ = nullptr;
        freeSlotList_ = nullptr;
        lastSlot_ = nullptr;
    }

    void* MemoryPool::allocate(){
        Slot* slt = popFreeList();
        

        if (slt != nullptr)
        return slt;

        Slot* temp;
        {
            if (curSlot_>lastSlot_)
            {
                allocateNewBlock();
            }
            
            temp = curSlot_;
            curSlot_+=slotSize_/sizeof(Slot);
        }

        return temp;

    }

    void MemoryPool::deallocate(void* ptr){
        if (!ptr)
        {
            return;
        }
        
        Slot* p = reinterpret_cast<Slot*> (ptr);
        pushFreelist(p);
    }

    void MemoryPool::allocateNewBlock(){
        void* new_block  = operator new(blockSize_);
        reinterpret_cast<Slot*>(new_block)->next = firstBlock_;
        firstBlock_ = reinterpret_cast<Slot*>(new_block);

        char* body = reinterpret_cast<char*>(new_block)+sizeof(Slot*);
        size_t pad = padPointer(body,slotSize_);
        curSlot_ = reinterpret_cast<Slot*>(body+pad);
        
        lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(new_block)+blockSize_-slotSize_+1);
    }

    size_t MemoryPool::padPointer(char* p, size_t align)
    {
    // align 是槽大小
    size_t rem = (reinterpret_cast<size_t>(p) % align);
    return rem == 0 ? 0 : (align - rem);
    }   

    bool MemoryPool::pushFreelist(Slot* slot){
        while (true)
        {
            Slot* old_head = freeSlotList_.load(std::memory_order_relaxed);
            slot->next.store(old_head,std::memory_order_relaxed);
            if(freeSlotList_.compare_exchange_weak(old_head,slot,std::memory_order_release,std::memory_order_relaxed)){
                return true;
            }
        }
        
    }

    Slot* MemoryPool::popFreeList(){
        while (true)
        {
            Slot* old_head = freeSlotList_.load(std::memory_order_relaxed);
                if (old_head == nullptr)
                return nullptr; // 队列为空

            // 在访问 newHead 之前再次验证 oldHead 的有效性
            Slot* new_head = nullptr;
            try
            {
                new_head = old_head->next.load(std::memory_order_relaxed);
            }
            catch(...)
            {
                // 如果返回失败，则continue重新尝试申请内存
                continue;
            }
            if(freeSlotList_.compare_exchange_weak(old_head,new_head,std::memory_order_acquire,std::memory_order_relaxed)){
                return old_head;
            }
        }
   
    }

    void HashBucket::init_MemoryPool(){
            for(int i=0;i<MEMORY_POOL_NUM;i++){
                    getMemoryPool(i).init((i+1)*SLOT_BASE_SIZE);
                }
    }

    MemoryPool& HashBucket::getMemoryPool(int index){
            static MemoryPool memorypool_[MEMORY_POOL_NUM];
            return memorypool_[index];
    }

}