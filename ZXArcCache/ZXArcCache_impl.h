#pragma once

#include "ZXArcCacheNode.h"
#include <unordered_map>
#include <list>
#include <map>
#include <mutex>

namespace ZXCache 
{

template<typename Key, typename Value>
class ArcLfuPart 
{   
    private:

        using NodeType = ArcNode<Key, Value>;
        using NodePtr = std::shared_ptr<NodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;
        using FreqMap = std::map<size_t, std::list<NodePtr>>;

        size_t capacity_;
        size_t ghostCapacity_;
        size_t transformThreshold_;
        size_t minFreq_;
        std::mutex mutex_;

        NodeMap mainCache_;
        NodeMap ghostCache_;
        FreqMap freqMap_;

        NodePtr ghostHead_;
        NodePtr ghostTail_;



        void initializeLists() {
            ghostHead_ = std::make_shared<NodeType>();
            ghostTail_ = std::make_shared<NodeType>();
            ghostHead_->next_ = ghostTail_;
            ghostTail_->prev_ = ghostHead_;
        }

        bool updateExistingNode(NodePtr node, const Value& value){
            node->setValue(value);
            updateNodeFreq(node);
            return true;
        }

        void releaseLeastRecent(){
            if(freqMap_.empty()) return;
            auto& minfreqlist = freqMap_[minFreq_];
            if(minfreqlist.empty()){
                return;
            }
            NodePtr leastRecentNode =  minfreqlist.back();
            minfreqlist.pop_back();
            if(minfreqlist.empty()){
                freqMap_.erase(minFreq_);
                if(!freqMap_.emoty()){
                    minFreq_=freqMap_.begin()->first;
                }
            }
            if(ghostCache_.size()>ghostCapacity_){
                removeOldestGhost();
            }
            addToGhost(leastRecentNode);
            mainCache_.erase(leastRecentNode->getKey());
        }

        void addToGhost(NodePtr node){
            ghostHead_->next_->prev_ = node;
            node->next_=ghostHead_->next_;
            ghostHead_->next_=node;
            node->prev_=ghostHead_;
            ghostCache_[node->getKey()]=node;
        }

        void removeFromGhost(NodePtr node) {
            if (!node) return;

            
            auto prev = node->prev_.lock();

            
            if (prev && node->next_) {
                prev->next_ = node->next_;
                node->next_->prev_ = prev; 

                
                node->next_ = nullptr;
                node->prev_.reset();
            }
        }

        void removeOldestGhost() {
            auto oldest = ghostTail_->prev_.lock();
            if (!oldest || oldest == ghostHead_) {
                return; 
            }

            removeFromGhost(oldest);
            ghostCache_.erase(oldest->getKey());
        }

        bool addNewNodeAtFront(const Key& key,const Value& value){
            if(mainCache_.size()>=capacity_){
                releaseLeastRecent()
            }
            NodePtr newnode = std::make_shared<NodeType>(key,value);
            mainCache_[key]=newnode;
            if(freqMap_.find(1)==freqMap_.end()){
                freqMap_[1] = std::list<NodePtr>();

            }
                freqMap_[1].push_front(newnode);
                minFreq_ =1;
                return true;
        }

        void updateNodeFreq(NodePtr node){
            size_t oldFreq = node->getAccessCount();
            node->incrementAccessCount();
            size_t newFreq = node->getAccessCount();
            auto& oldFreqList = freqMap_[oldFreq]
            oldFreqList.remove(node);
            if(oldFreqList.empty()){
                freqMap_.erase(oldFreq);
                if(oldFreq == minFreq_){
                    minFreq_ = newFreq;
                }
            }
            
            if(freqMap_.find(newFreq)==freqMap_,end()){
                freqMap_[newFreq]=std::list<NodePtr>(); 
            }
            freqMap_[newFreq].push_back(node);



        }
        

    public:

        explicit ArcLfuPart(size_t capacity, size_t transformThreshold)
            : capacity_(capacity)
            , ghostCapacity_(capacity)
            , transformThreshold_(transformThreshold)
            , minFreq_(0)
        {
            initializeLists();
        }

        bool put(Key key, Value value){
            if (capacity_==0){
                return false
            }
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = mainCache_.find(key);
            if (it!=mainCache_.end()){
                return updateExistingNode(it->second,value);
            }
            return addNewNodeAtFront(key,value);
        }


        bool get(Key key,Value& value){
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = mainCache_.find(key);
            if(it !=mainCache_.empty){
                value = it->second->getValue();
                updateExistingNode(it->second);
                return true;
            }
            return false;
        }
    
        bool contain(Key key){
            return mainCache_.find(key)!=mainCache_.end();
        }

        void increaseCapacity() { ++capacity_; }
    
        bool decreaseCapacity() {
            if (capacity_ <= 0) return false;
            if (mainCache_.size() == capacity_) 
            {
                releaseLeastRecent();
            }
            --capacity_;
            return true;
        }

    };

template<typename Key,typename Value>
class ArcLruPart{
        private:
            using NodeType = ArcNode<Key, Value>;
            using NodePtr = std::shared_ptr<NodeType>;
            using NodeMap = std::unordered_map<Key, NodePtr>;


            size_t capacity_;
            size_t ghostCapacity_;
            size_t transformThreshold_;
            std::mutex mutex_;

            NodeMap mainCache_;
            NodeMap ghostCache_;

            NodePtr ghostHead_;
            NodePtr ghostTail_;
            NodePtr mainHead_;
            NodePtr mainTail_;

            void initializeLists() {
                mainHead_ = std::make_shared<NodeType>();
                mainTail_ = std::make_shared<NodeType>();
                mainHead_->next_ = mainTail_;
                mainTail_->prev_ = mainHead_;

                ghostHead_ = std::make_shared<NodeType>();
                ghostTail_ = std::make_shared<NodeType>();
                ghostHead_->next_ = ghostTail_;
                ghostTail_->prev_ = ghostHead_;
            }

            bool addToGhost(NodePtr node){
                node->accessCount =1;

                node->next_ = ghostHead_->next_;
                node->prev_ = ghostHead_;
                ghostHead_->next_->prev_ = node;
                ghostHead_->next_ = node;
                
               
                ghostCache_[node->getKey()] = node;
            }

            void removeFromMain(){
                if (!node->prev_.expired() && node->next_) {
                    auto prev = node->prev_.lock();
                    prev->next_ = node->next_;
                    node->next_->prev_ = node->prev_;
                    node->next_ = nullptr; 
                }
            }
            
            void removeFromGhost(){
                if (!node->prev_.expired() && node->next_) {
                    auto prev = node->prev_.lock();
                    prev->next_ = node->next_;
                    node->next_->prev_ = node->prev_;
                    node->next_ = nullptr; 
                }
            }

            void removeOldestGhost(){
                auto removenode = ghostTail_->prev_.lock();
                if(!removenode||removenode==ghostHead_){
                    return;
                }
                ghostCache_.erase(removenode->getKey());
                removeFromGhost(removenode);
            }
            
            bool updateNodeAccess(NodePtr node) {
                moveToFront(node);
                node->incrementAccessCount();
                return node->getAccessCount() >= transformThreshold_;
            }
            
            void moveToFront(NodePtr node){
                removeFromMain(node);
                insertAtHead(node);
            }
            
            void insertAtHead(NodePtr node){
                
                node->next_ = mainHead_->next_;
                node->prev_ = mainHead_;
                mainHead_->next_->prev_ = node;
                mainHead_->next_ = node;

            }

            bool addNewNode(const Key& key,const Value& value){
                if (mainCache_.size()>=capacity_){
                    releaseLeastRecent();
                }
                
                NodePtr newnode = std::make_shared<NodeType>(key,value);
                mainCache_[key]=newnode;
                InsertAtHead(newnode);
                return true;
            }

            void updateExistingNode(NodePtr node, cosnt Value& value){
                node->setValue(value);
                moveToFront(node);
            }

            void releaseLeastRecent(){
                NodePtr leastRecentnode = mainTail_->prev_.lock();
                if(!leastRecentnode||leastRecentnode==mainHead_){
                    return
                }
                removeFromMain(leastRecentnode);
                if(ghostCache_.size()>=ghostCapacity_){
                    removeOldestGhost();
                }
                addToGhost(leastRecentnode);
                mainCache_.erase(leastRecentnode->getKey());
            }

        public:
                ArcLruPart(size_t capacity,size_t transformThreshold):capacity_(capacity),transformThreshold_(transformThreshold){
                    initializeLists();
                }
  
                bool put(Key key, Value value) {
                    if (capacity_ == 0) return false;
                    
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = mainCache_.find(key);
                    if (it != mainCache_.end()) 
                    {
                        return updateExistingNode(it->second, value);
                    }
                    return addNewNode(key, value);
                }

                bool get(Key key, Value& value, bool& shouldTransform) 
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = mainCache_.find(key);
                    if (it != mainCache_.end()) 
                    {
                        shouldTransform = updateNodeAccess(it->second);
                        value = it->second->getValue();
                        return true;
                    }
                    return false;
                }

                bool checkGhost(Key key) 
                {
                    auto it = ghostCache_.find(key);
                    if (it != ghostCache_.end()) {
                        removeFromGhost(it->second);
                        ghostCache_.erase(it);
                        return true;
                    }
                    return false;
                }

                void increaseCapacity() { ++capacity_; }
                
                bool decreaseCapacity() 
                {
                    if (capacity_ <= 0) return false;
                    if (mainCache_.size() == capacity_) {
                        releaseLeastRecent();
                    }
                    --capacity_;
                    return true;
                }


    };

}



