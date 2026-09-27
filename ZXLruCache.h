#pragma once

#include <cstring>
#include <list>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include "ZXCachePolicy.h"

namespace ZXCache 
{
    template<typename Key, typename Value> class ZXLruCache;
    template<typename Key, typename Value>
    class LruNode
    {
        private:
                Key key_;
                Value value_;
                size_t AccessCount_;
                std::shared_ptr<LruNode<Key,Value>> next_;
                std::weak_ptr<LruNode<Key,Value>> prev_;
        public:
                LruNode(Key key, Value value): key_(key), value_=(value), AccessCount_(1){}
                Key getKey() const {return key_;}
                Value getValue() const {return value_;}
                void  setValue(const Value& value){value_=value;}
                size_t getAccessCount() const{ return AccessCount_;}
                void   incrementAccessAcount() {++AccessCount_;}
                friend class  ZXLruCache<Key,Value>;
    };          

    template<typename Key, typename Value>
    class ZXLruCache: public ZXCachePolicy<Key,Value>
    {   
        private:
                using LruNodeType = LruNode<Key,Value>;
                using NodePtr = std::shared_ptr<LruNode<Key,Value>>;
                using NodeMap = std::unordered_map<Key,NodePtr>;
                int capacity_;
                NodePtr DummyHead;
                NodePtr DummyTail;
                std::mutex mutex_;
                NodeMap NodeMap_;
                void Initializer(){
                        DummyHead = std::make_shared<LruNodeType>(Key(),Value());
                        DummyTail = std::make_shared<LruNodeType>(Key(),Value());
                        DummyHead->next_ = DummyTail;
                        DummyTail->prev_ = DummyHead;
                };
                void DisconnectFromList(NodePtr nodeptr){
                        if (!nodeptr->prev_.expired()&&nodeptr->next_){
                                auto prevptr= nodeptr->prev_.lock();
                                prevptr->next_ = nodeptr->next_;
                                nodeptr->next_->prev_=nodeptr->prev_;
                        }
                        nodeptr->next_=nullptr;
                        nodeptr->prev_.reset();
                }
                void InsertAtFront(NodePtr nodeptr){

                        DummyHead->next_->prev_ = nodeptr;
                        nodeptr->next_ = DummyHead->next_;

                        DummyHead->next_= nodeptr;
                        nodeptr->prev_ = DummyHead;
                }

                void MovetoFront(NodePtr nodeptr){
                        DisconnectFromList(nodeptr);
                        InsertAtFront(nodeptr);
                }

                void ReleaseFromRear(){
                        NodePtr prevptr = DummyTail->prev_.lock();
                        if (!prevptr || prevptr==DummyHead){
                                return;
                        }
                        DisconnectFromList(prevptr);
                        NodeMap_.erase(prevptr->getKey());
                        //两处强引用被清除，离开这个函数作用域时，从列表被释放的对象自身根据RAII析构
                }

                void AddNodeToList(Key key, Value value){
                        if (NodeMap_.size()>=capacity_){
                                ReleaseFromRear();
                        }
                        NodePtr nodeptr = std::make_shared<LruNodeType>(key,value);
                        InsertAtFront(nodeptr);
                        NodeMap_[key]=nodeptr;
                }
        public:
                ZXLruCache(int capacity): capacity_(capacity) {
                        Initializer();
                }
                ~ZXLruCache() override = default;

                bool get(Key key, Value& value) override{
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto it = NodeMap_.find(key);
                        if (it!=NodeMap_.end()){
                                MovetoFront(it->second);
                                value = it->second->getValue();
                                return true;
                        }
                        else{
                                return false;
                        }
                }

                Value get(Key key) override{
                        Value value{};
                        get(key,value);
                        return value;
                }

                void put(Key key, Value value) override{
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (capacity_ <= 0) return;
                        auto it = NodeMap_.find(key);
                        if(it!=NodeMap_.end()){
                                it->second->setValue(value);
                                MovetoFront(it->second);
                        }
                        else{
                                AddNodeToList(key,value);
                        }
                }

    };
template<typename Key, typename Value>
class ZXHashLruCaches{
        public:
        ZXHashLruCaches(size_t capacity, int sliceNum)
                : capacity_(capacity)
                , sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
        {
                size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum_)); // 获取每个分片的大小
                for (int i = 0; i < sliceNum_; ++i)
                {
                lruSliceCaches_.emplace_back(new ZXLruCache<Key, Value>(sliceSize)); 
                }
        }

        void put(Key key, Value value)
        {
                
                size_t sliceIndex = Hash(key) % sliceNum_;
                lruSliceCaches_[sliceIndex]->put(key, value);
        }

        bool get(Key key, Value& value)
        {
               
                size_t sliceIndex = Hash(key) % sliceNum_;
                return lruSliceCaches_[sliceIndex]->get(key, value);
        }

        Value get(Key key)
        {
                Value value;
                //memset(&value, 0, sizeof(value));
                get(key, value);
                return value;
        }

        private:
        // 将key转换为对应hash值
        size_t Hash(Key key)
        {
                std::hash<Key> hashFunc;
                return hashFunc(key);
        }

        private:
        size_t                                              capacity_;  // 总容量
        int                                                 sliceNum_;  // 切片数量
        std::vector<std::unique_ptr<ZXLruCache<Key, Value>>> lruSliceCaches_; // 切片LRU缓存
 };
} 