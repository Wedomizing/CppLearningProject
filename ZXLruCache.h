#pragma once

#include <cstring>
#include <list>
#include <unordered_map>
#include <memory>
#include <mutex>
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
                using NodePtr = std::shared_ptr<LruNode<Key,Value>;
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
                        //lock 

                }
                void MovetoFront(NodePtr nodeptr){
                        DisconnectFromList(nodeptr);
                        InsertAtFront(nodeptr);
                }


        public:
                ZXLruCache(int capacity):capacity_=capacity{
                        Initializer();
                }

                bool get(Key key, Value& value){
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

                Value get(Key key){

                }

                void push(Key key, Value value){

                }


    };
    



} // namespace ZXCache
