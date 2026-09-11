#pragma once

#include <cstring>
#include <list>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "ZXCachePolicy.h"

namespace ZXCache 
{
    template<typename Key, typename Value> class ZXLfuCache;
    template<typename Key, typename Value>
    class FreqList
    {   
        private:
                struct BaseNode{
                        int freq_node;
                        Key key_;
                        Value value_;
                        std::weak_ptr<BaseNode> prev_node;
                        std::shared_ptr<BaseNode> next_node;
                };
                BaseNode(): freq(1), next_(nullptr){}
                BaseNode(Key key, Value value): key_(key),value_(Value),freq(1),next(nullptr) {}

                using NodePtr = std::shared_ptr<BaseNode>;

                int freq_list;
                NodePtr head_;
                NodePtr tail_;

        public:
                explicit FreqList(int n):freq_list(n){
                        head_=std::make_shared<BaseNode>();
                        tail_=std::make_shared<BaseNode>();
                        head_->next_node = tail_;
                        tail_->prev_node = head_;
                }

                bool isEmpty() const
                {
                return head_->next_node == tail_;
                }
                


                NodePtr getFront() const { return head_->next; }


                friend class  ZXLfuCache<Key,Value>;
    };          

    
} 