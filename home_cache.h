#ifndef SUITEOSD_HOME_CACHE_H
#define SUITEOSD_HOME_CACHE_H
#include <map>
#include <cstdint>
#include <cstddef>
#include <utility>
namespace suiteosd {
// Small bounded concurrent working set; not a server-side request gate.
inline constexpr std::size_t kHomePosterWorkers=4;
// A bounded Home working set with stable node addresses. Evict individual
// least-recently used entries, never the entire cache on a capacity miss.
template<class Key,class Value> class HomeLru {
    std::map<Key,Value> entries;
    std::map<Key,std::uint64_t> used;
    std::uint64_t clock=0;
    void touch(const Key &key){used[key]=++clock;}
public:
    auto find(const Key &key){
        auto it=entries.find(key);if(it!=entries.end())touch(key);return it;
    }
    auto find(const Key &key) const{return entries.find(key);}
    auto end(){return entries.end();}
    auto end() const{return entries.end();}
    auto size() const{return entries.size();}
    template<class F> std::size_t totalWeight(F weight) const {
        std::size_t sum=0;for(const auto &entry:entries)sum+=weight(entry.second);return sum;
    }
    void clear(){entries.clear();used.clear();clock=0;}
    template<class V> auto emplace(const Key &key,V &&v){
        auto result=entries.emplace(key,std::forward<V>(v));touch(key);return result;
    }
    template<class V> void insertOrAssign(const Key &key,V &&v){
        entries.insert_or_assign(key,std::forward<V>(v));touch(key);
    }
    void erase(const Key &key){entries.erase(key);used.erase(key);}
    template<class F> void evictOldest(F beforeErase){
        if(used.empty())return;
        auto oldest=used.begin();
        for(auto it=used.begin();it!=used.end();++it)if(it->second<oldest->second)oldest=it;
        auto entry=entries.find(oldest->first);
        beforeErase(entry->first,entry->second);
        entries.erase(entry);used.erase(oldest);
    }
    void evictOldest(){evictOldest([](const Key &,const Value &){});}
    template<class F> void eraseIf(F predicate){
        for(auto it=entries.begin();it!=entries.end();){
            if(predicate(it->first,it->second)){used.erase(it->first);it=entries.erase(it);}
            else ++it;
        }
    }
};
}
#endif
