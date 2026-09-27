#pragma once

#include "../ZXCachePolicy.h"
#include "ZXArcCache_impl.h"
#include <memory>

namespace ZXCache 
{

template<typename Key, typename Value>
class ZXArcCache : public ZXCachePolicy<Key, Value> 
{
    private:
            size_t capacity_;
            size_t transformThreshold_;
            std::unique_ptr<ArcLruPart<Key, Value>> lruPart_;
            std::unique_ptr<ArcLfuPart<Key, Value>> lfuPart_;

            
    public:
            explicit ZXArcCache(size_t capacity=10,size_t transformThreshold=2):
                capacity_(capacity),
                transformThreshold_(transformThreshold),
                lfuPart_(std::make_unique<ArcLfuPart<Key,Value>>(capacity_,transformThreshold_)),
                lruPart_(std::make_unique<ArcLruPart<Key,Value>>(capacity_,transformThreshold_)){}

                ~ZXArcCache() override = default;

                bool checkGhostCaches(Key key) {
                    bool inGhost = false;
                    if (lruPart_->checkGhost(key)) 
                    {
                        if (lfuPart_->decreaseCapacity()) 
                        {
                            lruPart_->increaseCapacity();
                        }
                        inGhost = true;
                    } 
                    else if (lfuPart_->checkGhost(key)) 
                    {
                        if (lruPart_->decreaseCapacity()) 
                        {
                            lfuPart_->increaseCapacity();
                        }
                        inGhost = true;
                    }
                    return inGhost;
                }

                void put(Key key, Value value) override{
                    checkGhostCaches(key);
                    bool lfuhit = lfuPart_->contain(key);
                    lruPart_->put(key,value);
                    if(lfuhit){
                        lfuPart_->put(key,value);
                    }
                }
                bool get(Key key,Value& value) override {
                    checkGhostCaches(key);
                    bool Transform = false;
                    if(lruPart_->get(key,value,Transform)){
                        if(Transform){
                            lfuPart_->put(key,value);
                        }
                        return true;
                    }
                    return lfuPart_->get(key,value);
                }
                Value get(Key key) override 
                {
                    Value value{};
                    get(key, value);
                    return value;
                }

};


}