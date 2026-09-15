#pragma once

#include <cstdint>
#include <cstring>
#include <climits>
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
                        BaseNode(): freq_node(1), next_node(nullptr){}
                        BaseNode(Key key, Value value): key_(key),value_(value),freq_node(1),next_node(nullptr) {}
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

                NodePtr getRear() const { 
                        
                        auto rear = tail_->prev_node.lock();
                        if(rear == head_) return nullptr;
                        return rear; 
                }

                void insertNodeAtFront(NodePtr nodeptr){
                        if(!nodeptr||!head_||!tail_) return;
                        head_->next_node->prev_node = nodeptr;
                        nodeptr->next_node = head_->next_node;
                        nodeptr->prev_node = head_;
                        head_->next_node = nodeptr;
                }

                void disconnectFromFreqList(NodePtr nodeptr){
                        if (!nodeptr || !head_ || !tail_) return;
                        if (nodeptr->prev_node.expired()||!nodeptr->next_node) return;
                        auto prev_node_ptr = nodeptr->prev_node.lock();
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
                std::unordered_map<int, std::unique_ptr<FreqList<Key,Value>>> freqToFreqList_;// 访问频次到该频次链表的映射

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
                        minFreq_(INT_MAX),curAverageNum_(0),curTotalNum_(0){}


                void put(Key key, Value value) override{
                        std::lock_guard<std::mutex> lock(mutex_);
                        if(capacity_==0) return;
                        auto it = nodeMap_.find(key);
                        if(it!=nodeMap_.end()){
                                it->second->value_=value;
                                getInternal(it->second,value);
                                return;
                        }
                        putInternal(key,value);
                }
                
                bool get(Key key, Value& value) override{
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto it = nodeMap_.find(key);
                        if (it!=nodeMap_.end()){

                                getInternal(it->second,value);

                                return true;
                        }
                        else{
                                return false;
                        }
                }

                Value get(Key key) override{
                        Value value;
                        get(key,value);
                        return value;
                }
                void purge(){
                        std::lock_guard<std::mutex> lock(mutex_);
                        nodeMap_.clear();
                        freqToFreqList_.clear();
                        minFreq_ = INT_MAX;
                        curAverageNum_ = 0;
                        curTotalNum_ = 0;
                }

    };

        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::getInternal(NodePtr node, Value& value){
                value=node->value_;
                removeFromFreqList(node);
                node->freq_node++;
                addToFreqList(node);
                //如果由于这个新被访问的节点向更高频次移动，导致原频次列表现为空列表，则提升最低访问
                if (node->freq_node - 1 == minFreq_ && freqToFreqList_[node->freq_node - 1]->isEmpty()) 
                        minFreq_++;
                
                addFreqNum();
        }

        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::putInternal(Key key, Value value){
                if (nodeMap_.size()==capacity_){
                        kickOut();
                }
                NodePtr newnode = std::make_shared<Node>(key,value);
                nodeMap_[key]=newnode;
                addToFreqList(newnode);
                addFreqNum();
                minFreq_ = std::min(minFreq_,1);
                
        }

        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::kickOut(){

                auto it = freqToFreqList_.find(minFreq_);
                if (it == freqToFreqList_.end() || !it->second) return;
                if(!node) return;            
                removeFromFreqList(node);
                nodeMap_.erase(node->key_);
                decreaseFreqNum(node->freq_node);

        }


        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::removeFromFreqList(NodePtr node){
                if(!node) return;
                auto freq = node->freq_node;
                freqToFreqList_[freq]->disconnectFromFreqList(node);
        }


        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::addToFreqList(NodePtr node){
                if(!node) return;
                auto freq = node->freq_node;
                if(freqToFreqList_.find(freq)==freqToFreqList_.end()){
                        freqToFreqList_[freq]= std::make_unique<FreqList<Key,Value>>(freq);
                }
                freqToFreqList_[freq]->insertNodeAtFront(node);
        }

        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::addFreqNum(){
                curTotalNum_++;
                if(nodeMap_.empty()){
                        curAverageNum_=0;
                }
                else{
                        curAverageNum_ = curTotalNum_/nodeMap_.size();
                }
                if(curAverageNum_>=maxAverageNum_){
                        handleOverMaxAverageNum();
                }
        }
        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::decreaseFreqNum(int freq){
                curTotalNum_-=freq;
                if(nodeMap_.empty()){
                        curAverageNum_= 0;
                }
                else{
                        curAverageNum_ = curTotalNum_/nodeMap_.size();
                }
        }
        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::handleOverMaxAverageNum(){
                if(nodeMap_.empty()) return;
                for(auto it = nodeMap_.begin();it!=nodeMap_.end();it++){
                        if(!it->second) continue;
                        NodePtr node = it->second;
                        int freqold = node->freq_node;
                        removeFromFreqList(node);
                        node->freq_node -=maxAverageNum_/2;//以最大值的一半衰减当前频率
                        if(node->freq_node<1) node->freq_node=1;
                       
                        int freqdiff = node->freq_node-freqold;
                        
                        
                        curTotalNum_+=freqdiff;
                        addToFreqList(node);
                }
                updateMinFreq();
        }
        template<typename Key, typename Value>
        void ZXLfuCache<Key, Value>::updateMinFreq(){
                minFreq_ = INT_MAX;
                for(const auto& pair:freqToFreqList_){
                        if(pair.second&&!pair.second->isEmpty()){
                                minFreq_ = std::min(pair.first,minFreq_);
                        }
                }
                if (minFreq_ == INT_MAX){
                        minFreq_=1;
                }
        }
} 