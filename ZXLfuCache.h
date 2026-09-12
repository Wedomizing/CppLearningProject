#pragma once

#include <cstring>
#include <list>
#include <unordered_map>
#include <memory>
#include <mutex>
#include "ZXCachePolicy.h"

namespace ZXCache 
{
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
                        BaseNode(): freq(1), next_(nullptr){}
                        BaseNode(Key key, Value value): key_(key),value_(Value),freq(1),next(nullptr) {}
                };
                

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

                void insertNodeAtFront(NodePtr nodeptr){
                        if(!nodeptr||!head_||!tail_) return;
                        head_->next_node->prev_node = nodeptr;
                        nodeptr->next_node = head->next_node;
                        nodepte->prev_node = head_;
                        head_->next = nodeptr;
                }

                void disconnectFromFreqList(Nodeptr nodeptr){
                        if (!node || !head_ || !tail_) return;
                        if (node->prev_node.expired()||!node->next_node) return;
                        auto prev_node_ptr = node->prev_node.lock();
                        prev_node_ptr->next_node = nodeptr->next_node;
                        nodeptr->prev_node.reset();
                        nodeptr->next_node->prev_node = prev_node_ptr;
                        nodeptr->next_node = nullptr;
                }

                friend class  ZXLfuCache<Key,Value>;
    };          

    template <typename Key, typename Value>
    class ZXLfuCache : public ZXCachePolicy<Key, Value>{
        private:
                using Node = typename FreqList<Key, Value>::BaseNode;
                using NodePtr = std::shared_ptr<Node>;
                using NodeMap = std::unordered_map<Key, NodePtr>;
                int                                            capacity_; // 缓存容量
                int                                            minFreq_; // 最小访问频次(用于找到最小访问频次结点)
                int                                            maxAverageNum_; // 最大平均访问频次
                int                                            curAverageNum_; // 当前平均访问频次
                int                                            curTotalNum_; // 当前访问所有缓存次数总数 
                std::mutex                                     mutex_; // 互斥锁
                NodeMap                                        nodeMap_; // key 到 缓存节点的映射
                std::unordered_map<int, FreqList<Key, Value>*> freqToFreqList_;// 访问频次到该频次链表的映射

                void putInternal(Key key, Value value); // 添加缓存
                void getInternal(NodePtr node, Value& value); // 获取缓存


                void kickOut(); // 移除缓存中的过期数据
                void removeFromFreqList(NodePtr node); // 从频率列表中移除节点

                void addToFreqList(NodePtr node); // 添加到频率列表

                void addFreqNum(); // 增加平均访问等频率
                void decreaseFreqNum(int num); // 减少平均访问等频率

                void handleOverMaxAverageNum(); // 处理当前平均访问频率超过上限的情况

                void updateMinFreq();



        public:
                ZXLfuCache(int capacity,int maxAverageNum=1000000):
                        capacity_(capacity),maxAverageNum_(maxAverageNum),
                        minFreq_(INT8_MIN),curAverageNum_(0),curTotalNum_(0){}


                void put(Key key, Value value){
                        std::lock_guard<std::mutex> lock(mutex_);
                        if(capacity_==0) return;
                        auto it = nodeMap_.find(key);
                        if(it!=nodeMap_.end()){
                                it->second->value_=value;
                                getInternal(it->second,value);
                        }
                        putInternal(key,value);
                }
                
                bool get(Key key, Value& value){
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto it = nodeMap_.find(key);
                        if (it!=nodeMap_.end()){
                                getInternal(it->second,value)
                                value = it->second->value_;
                                return true;
                        }
                        else{
                                return false;
                        }
                }

                Value get(Key key){
                        Value value;
                        get(key,value);
                        return value;
                }

    };

        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::getInternal(NodePtr node, Value& value){
                
        }


} 