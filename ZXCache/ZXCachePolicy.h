#pragma once

namespace ZXCache{

    template<typename Key, typename Value> 
    class ZXCachePolicy
    {

    public:
            virtual ~ZXCachePolicy() {};

            virtual void put(Key key, Value value) = 0;
            virtual bool get(Key key, Value& value) = 0;
            virtual Value get(Key key) =0;


    };
}