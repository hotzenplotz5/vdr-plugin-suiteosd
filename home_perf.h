#pragma once
#include <atomic>
#include <chrono>
namespace suiteosd {
struct HomePerfCounters {
    std::atomic<unsigned long long> http{0}, httpBytes{0}, httpUs{0}, connections{0}, decodes{0}, decodeUs{0};
};
inline HomePerfCounters homePerf;
struct DecodePerf {
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    ~DecodePerf(){
        ++homePerf.decodes;
        homePerf.decodeUs+=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    }
};
}
