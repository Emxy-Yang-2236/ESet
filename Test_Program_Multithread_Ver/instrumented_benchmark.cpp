#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <functional>
#include <stdexcept>
#include "../src.hpp"

using namespace std;
using Clock = chrono::steady_clock;

struct Counters {
    uint64_t comparisons = 0;
    uint64_t allocations = 0;
    uint64_t allocated_bytes = 0;
};

static Counters g_counters;
static bool g_count_alloc = false;
static volatile uint64_t global_sink = 0;

struct CountingLess {
    bool operator()(const int& a, const int& b) const {
        ++g_counters.comparisons;
        return a < b;
    }
};

void* operator new(size_t size) {
    if (g_count_alloc) {
        ++g_counters.allocations;
        g_counters.allocated_bytes += size;
    }
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

void* operator new[](size_t size) {
    if (g_count_alloc) {
        ++g_counters.allocations;
        g_counters.allocated_bytes += size;
    }
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc();
}

void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }

#ifdef __cpp_aligned_new
void* operator new(size_t size, std::align_val_t align) {
    if (g_count_alloc) {
        ++g_counters.allocations;
        g_counters.allocated_bytes += size;
    }
    void* p = nullptr;
    size_t alignment = static_cast<size_t>(align);
    if (posix_memalign(&p, alignment, size) == 0) return p;
    throw std::bad_alloc();
}
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, size_t, std::align_val_t) noexcept { std::free(p); }
#endif

template<class F>
static pair<long long, Counters> measure_instrumented(F&& f) {
    g_counters = Counters{};
    g_count_alloc = true;
    auto begin = Clock::now();
    f();
    auto end = Clock::now();
    g_count_alloc = false;
    long long ns = chrono::duration_cast<chrono::nanoseconds>(end - begin).count();
    return {ns, g_counters};
}

static vector<int> make_random_unique(int n, uint64_t seed) {
    vector<int> v(n);
    iota(v.begin(), v.end(), 0);
    mt19937_64 rng(seed);
    shuffle(v.begin(), v.end(), rng);
    return v;
}

static vector<int> make_queries(int n, int lo, int hi, uint64_t seed) {
    vector<int> v(n);
    mt19937_64 rng(seed);
    uniform_int_distribution<int> dist(lo, hi);
    for (int& x : v) x = dist(rng);
    return v;
}

template<class Container>
static Container build_no_count(const vector<int>& data) {
    bool old = g_count_alloc;
    g_count_alloc = false;
    Container c;
    for (int x : data) c.emplace(x);
    g_count_alloc = old;
    return c;
}

template<class Container>
static size_t range_count(const Container& c, int l, int r) {
    if (r < l) return 0;
    if constexpr (std::is_same_v<Container, ESet<int, CountingLess>>) {
        return c.range(l, r);
    } else {
        return static_cast<size_t>(distance(c.lower_bound(l), c.upper_bound(r)));
    }
}

static void print_row(const string& container, const string& scenario, int n, long long ns, const Counters& c, uint64_t checksum) {
    cout << container << ',' << scenario << ',' << n << ',' << ns << ','
         << c.comparisons << ',' << c.allocations << ',' << c.allocated_bytes << ',' << checksum << '\n';
}

template<class Container>
static void run_for(const string& name, int n, uint64_t seed) {
    auto data = make_random_unique(n, seed);
    auto hit_queries = make_random_unique(n, seed + 1);
    auto miss_queries = make_queries(n, 2 * n, 4 * n, seed + 2);
    auto starts = make_queries(n, 0, max(0, n - 1), seed + 3);

    {
        uint64_t checksum = 0;
        auto [ns, cnt] = measure_instrumented([&] {
            Container c;
            for (int x : data) {
                auto r = c.emplace(x);
                checksum += r.second;
            }
            checksum += c.size();
        });
        print_row(name, "insert_random_unique", n, ns, cnt, checksum);
        global_sink += checksum;
    }

    Container base = build_no_count<Container>(data);

    {
        uint64_t checksum = 0;
        auto [ns, cnt] = measure_instrumented([&] {
            for (int x : hit_queries) {
                auto it = base.find(x);
                if (it != base.end()) checksum += static_cast<uint64_t>(*it);
            }
        });
        print_row(name, "find_hit", n, ns, cnt, checksum);
        global_sink += checksum;
    }

    {
        uint64_t checksum = 0;
        auto [ns, cnt] = measure_instrumented([&] {
            for (int x : miss_queries) {
                auto it = base.find(x);
                if (it != base.end()) checksum += static_cast<uint64_t>(*it);
            }
        });
        print_row(name, "find_miss", n, ns, cnt, checksum);
        global_sink += checksum;
    }

    {
        uint64_t checksum = 0;
        auto [ns, cnt] = measure_instrumented([&] {
            Container c = base; // copy is intentionally outside erase loop? It is inside measured block here.
            for (int x : hit_queries) checksum += c.erase(x);
            checksum += c.size();
        });
        print_row(name, "copy_then_erase_all_random", n, ns, cnt, checksum);
        global_sink += checksum;
    }

    {
        uint64_t checksum = 0;
        auto [ns, cnt] = measure_instrumented([&] {
            for (int l : starts) checksum += range_count(base, l, l + 1);
        });
        print_row(name, "range_width_1", n, ns, cnt, checksum);
        global_sink += checksum;
    }

    {
        uint64_t checksum = 0;
        int width = max(1, n / 2);
        auto [ns, cnt] = measure_instrumented([&] {
            for (int l : starts) checksum += range_count(base, l, l + width);
        });
        print_row(name, "range_width_n_over_2", n, ns, cnt, checksum);
        global_sink += checksum;
    }

    {
        uint64_t checksum = 0;
        auto [ns, cnt] = measure_instrumented([&] {
            Container c(base);
            checksum += c.size();
        });
        print_row(name, "copy_constructor", n, ns, cnt, checksum);
        global_sink += checksum;
    }
}

int main(int argc, char** argv) {
    int n = 200000;
    uint64_t seed = 20260426;
    string which = "both";

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        auto value = [&](const string& name) -> string {
            if (i + 1 >= argc) { cerr << "missing value for " << name << "\n"; exit(2); }
            return argv[++i];
        };
        if (arg == "--n") n = stoi(value(arg));
        else if (arg == "--seed") seed = stoull(value(arg));
        else if (arg == "--container") which = value(arg);
        else if (arg == "--help") {
            cout << "Usage: ./instrumented_benchmark [--n N] [--seed S] [--container eset|stl|both]\n";
            return 0;
        } else {
            cerr << "unknown arg: " << arg << "\n";
            return 2;
        }
    }

    cout << "container,scenario,n,total_ns,comparisons,allocations,allocated_bytes,checksum\n";
    if (which == "eset" || which == "both") run_for<ESet<int, CountingLess>>("ESet", n, seed);
    if (which == "stl" || which == "both") run_for<std::set<int, CountingLess>>("std::set", n, seed);
    cerr << "global_sink=" << global_sink << "\n";
    return 0;
}
