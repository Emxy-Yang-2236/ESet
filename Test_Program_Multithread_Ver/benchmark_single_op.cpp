#include <algorithm>
#include <array>
#include <atomic>
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
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <functional>
#include <stdexcept>
#include "../src.hpp"

using namespace std;
using Clock = chrono::steady_clock;
static atomic<uint64_t> global_sink{0};
static int g_jobs = 1;

struct TrialResult {
    long long ns = 0;
    uint64_t checksum = 0;
};

template<class Container>
static size_t range_count(const Container& c, int l, int r) {
    if (r < l) return 0;
    if constexpr (std::is_same_v<Container, ESet<int>>) {
        return c.range(l, r);
    } else {
        return static_cast<size_t>(distance(c.lower_bound(l), c.upper_bound(r)));
    }
}

template<class F>
static long long measure_ns(F&& f) {
    auto begin = Clock::now();
    f();
    auto end = Clock::now();
    return chrono::duration_cast<chrono::nanoseconds>(end - begin).count();
}

template<class F>
static pair<vector<long long>, uint64_t> run_trials_parallel(int repeat, int jobs, F&& trial) {
    vector<long long> times(repeat);
    vector<uint64_t> checks(repeat);
    if (repeat <= 0) return {times, 0};
    jobs = max(1, min(jobs, repeat));
    if (jobs == 1) {
        for (int r = 0; r < repeat; ++r) {
            TrialResult tr = trial(r);
            times[r] = tr.ns;
            checks[r] = tr.checksum;
        }
    } else {
        atomic<int> next{0};
        vector<thread> workers;
        workers.reserve(jobs);
        for (int j = 0; j < jobs; ++j) {
            workers.emplace_back([&] {
                while (true) {
                    int r = next.fetch_add(1, memory_order_relaxed);
                    if (r >= repeat) break;
                    TrialResult tr = trial(r);
                    times[r] = tr.ns;
                    checks[r] = tr.checksum;
                }
            });
        }
        for (auto& th : workers) th.join();
    }
    uint64_t checksum = 0;
    for (auto x : checks) checksum += x;
    global_sink.fetch_add(checksum, memory_order_relaxed);
    return {times, checksum};
}

static vector<int> make_ascending(int n) {
    vector<int> v(n);
    iota(v.begin(), v.end(), 0);
    return v;
}

static vector<int> make_descending(int n) {
    vector<int> v = make_ascending(n);
    reverse(v.begin(), v.end());
    return v;
}

static vector<int> make_random_unique(int n, uint64_t seed) {
    vector<int> v = make_ascending(n);
    mt19937_64 rng(seed);
    shuffle(v.begin(), v.end(), rng);
    return v;
}

static vector<int> make_random_queries(int n, int lo, int hi, uint64_t seed) {
    vector<int> v(n);
    mt19937_64 rng(seed);
    uniform_int_distribution<int> dist(lo, hi);
    for (int& x : v) x = dist(rng);
    return v;
}

template<class Container>
static Container build_container(const vector<int>& data) {
    Container c;
    for (int x : data) c.emplace(x);
    return c;
}

static void print_row(const string& container, int n, int repeat, const string& op, const string& pattern,
                      const vector<long long>& times, uint64_t checksum, long long logical_ops) {
    double mean = accumulate(times.begin(), times.end(), 0.0) / max<size_t>(1, times.size());
    double var = 0;
    for (auto x : times) var += (x - mean) * (x - mean);
    var /= max<size_t>(1, times.size() > 1 ? times.size() - 1 : 1);
    double sd = sqrt(var);
    double avg = logical_ops > 0 ? mean / logical_ops : mean;
    cout << container << ',' << n << ',' << repeat << ',' << op << ',' << pattern << ','
         << fixed << setprecision(2) << mean << ',' << sd << ',' << avg << ',' << checksum << '\n';
}

template<class Container>
static void bench_insert(const string& name, int n, int repeat, uint64_t seed) {
    vector<pair<string, vector<int>>> patterns;
    patterns.push_back({"random_unique", make_random_unique(n, seed)});
    patterns.push_back({"ascending", make_ascending(n)});
    patterns.push_back({"descending", make_descending(n)});

    vector<int> dup(n);
    for (int i = 0; i < n; ++i) dup[i] = i % max(1, n / 10);
    mt19937_64 rng(seed + 17);
    shuffle(dup.begin(), dup.end(), rng);
    patterns.push_back({"many_duplicates", dup});

    for (auto& item : patterns) {
        const string& pat = item.first;
        const vector<int>& data = item.second;
        auto [times, checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
            uint64_t local = 0;
            long long ns = measure_ns([&] {
                Container c;
                for (int x : data) {
                    auto res = c.emplace(x);
                    local += res.second;
                }
                local += c.size();
            });
            return TrialResult{ns, local};
        });
        print_row(name, n, repeat, "emplace", pat, times, checksum, n);
    }
}

template<class Container>
static void bench_erase(const string& name, int n, int repeat, uint64_t seed) {
    auto data = make_random_unique(n, seed);
    vector<pair<string, vector<int>>> patterns;
    patterns.push_back({"existing_random", data});
    patterns.push_back({"existing_ascending", make_ascending(n)});
    patterns.push_back({"existing_descending", make_descending(n)});
    patterns.push_back({"missing", make_random_queries(n, 2 * n, 4 * n, seed + 1)});

    for (auto& item : patterns) {
        const string& pat = item.first;
        const vector<int>& keys = item.second;
        auto [times, checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
            uint64_t local = 0;
            Container c = build_container<Container>(data);
            long long ns = measure_ns([&] {
                for (int x : keys) local += c.erase(x);
            });
            local += c.size();
            return TrialResult{ns, local};
        });
        print_row(name, n, repeat, "erase", pat, times, checksum, n);
    }
}

template<class Container>
static void bench_query(const string& name, int n, int repeat, uint64_t seed) {
    auto data = make_random_unique(n, seed);
    vector<pair<string, vector<int>>> patterns;
    patterns.push_back({"hit", make_random_unique(n, seed + 2)});
    patterns.push_back({"miss", make_random_queries(n, 2 * n, 4 * n, seed + 3)});

    vector<int> mixed;
    mixed.reserve(n);
    auto hit = make_random_unique(n / 2, seed + 4);
    auto miss = make_random_queries(n - n / 2, 2 * n, 4 * n, seed + 5);
    mixed.insert(mixed.end(), hit.begin(), hit.end());
    mixed.insert(mixed.end(), miss.begin(), miss.end());
    mt19937_64 rng(seed + 6);
    shuffle(mixed.begin(), mixed.end(), rng);
    patterns.push_back({"half_hit_half_miss", mixed});

    for (auto& item : patterns) {
        const string& pat = item.first;
        const vector<int>& queries = item.second;
        for (string op : {"find", "lower_bound", "upper_bound"}) {
            auto [times, checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
                uint64_t local = 0;
                Container c = build_container<Container>(data);
                long long ns = measure_ns([&] {
                    for (int x : queries) {
                        if (op == "find") {
                            auto it = c.find(x);
                            if (it != c.end()) local += static_cast<uint64_t>(*it);
                        } else if (op == "lower_bound") {
                            auto it = c.lower_bound(x);
                            if (it != c.end()) local += static_cast<uint64_t>(*it);
                        } else {
                            auto it = c.upper_bound(x);
                            if (it != c.end()) local += static_cast<uint64_t>(*it);
                        }
                    }
                });
                return TrialResult{ns, local};
            });
            print_row(name, n, repeat, op, pat, times, checksum, queries.size());
        }
    }
}

template<class Container>
static void bench_range(const string& name, int n, int repeat, uint64_t seed) {
    auto data = make_random_unique(n, seed);
    vector<pair<string, int>> widths = {
        {"width_1", 1},
        {"width_32", 32},
        {"width_1024", 1024},
        {"width_n_over_2", max(1, n / 2)}
    };
    int q = n;
    auto starts = make_random_queries(q, 0, max(0, n - 1), seed + 7);

    for (auto [pat, width] : widths) {
        auto [times, checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
            uint64_t local = 0;
            Container c = build_container<Container>(data);
            long long ns = measure_ns([&] {
                for (int l : starts) {
                    int rr = l + width;
                    local += range_count(c, l, rr);
                }
            });
            return TrialResult{ns, local};
        });
        print_row(name, n, repeat, "range", pat, times, checksum, q);
    }
}

template<class Container>
static void bench_iterator(const string& name, int n, int repeat, uint64_t seed) {
    auto data = make_random_unique(n, seed);

    auto [forward_times, forward_checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
        uint64_t local = 0;
        Container c = build_container<Container>(data);
        long long ns = measure_ns([&] {
            for (auto it = c.begin(); it != c.end(); ++it) local += static_cast<uint64_t>(*it);
        });
        return TrialResult{ns, local};
    });

    auto [reverse_times, reverse_checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
        uint64_t local = 0;
        Container c = build_container<Container>(data);
        long long ns = measure_ns([&] {
            if (c.size() == 0) return;
            auto it = c.end();
            while (it != c.begin()) {
                --it;
                local += static_cast<uint64_t>(*it);
            }
        });
        return TrialResult{ns, local};
    });

    print_row(name, n, repeat, "iterator", "forward_full_traversal", forward_times, forward_checksum, n);
    print_row(name, n, repeat, "iterator", "reverse_full_traversal", reverse_times, reverse_checksum, n);
}

template<class Container>
static void bench_copy_move(const string& name, int n, int repeat, uint64_t seed) {
    auto data = make_random_unique(n, seed);
    Container base = build_container<Container>(data);

    auto [copy_ctor_times, copy_ctor_checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
        uint64_t local = 0;
        long long ns = measure_ns([&] {
            Container c(base);
            local += c.size();
        });
        return TrialResult{ns, local};
    });

    auto [copy_assign_times, copy_assign_checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
        uint64_t local = 0;
        long long ns = measure_ns([&] {
            Container c;
            c = base;
            local += c.size();
        });
        return TrialResult{ns, local};
    });

    int move_sources = 8;
    auto [move_ctor_times, move_ctor_checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
        uint64_t local = 0;
        vector<Container> sources;
        sources.reserve(move_sources);
        for (int k = 0; k < move_sources; ++k) sources.push_back(base);
        long long ns = measure_ns([&] {
            vector<Container> dst;
            dst.reserve(move_sources);
            for (int k = 0; k < move_sources; ++k) {
                Container moved(std::move(sources[k]));
                local += moved.size();
                dst.push_back(std::move(moved));
            }
        });
        return TrialResult{ns, local};
    });

    auto [move_assign_times, move_assign_checksum] = run_trials_parallel(repeat, g_jobs, [&](int) {
        uint64_t local = 0;
        vector<Container> sources;
        sources.reserve(move_sources);
        for (int k = 0; k < move_sources; ++k) sources.push_back(base);
        long long ns = measure_ns([&] {
            vector<Container> dst(move_sources);
            for (int k = 0; k < move_sources; ++k) {
                dst[k] = std::move(sources[k]);
                local += dst[k].size();
            }
        });
        return TrialResult{ns, local};
    });

    print_row(name, n, repeat, "copy", "copy_constructor", copy_ctor_times, copy_ctor_checksum, n);
    print_row(name, n, repeat, "copy", "copy_assignment", copy_assign_times, copy_assign_checksum, n);
    print_row(name, n, repeat, "move", "move_constructor_isolated", move_ctor_times, move_ctor_checksum, move_sources);
    print_row(name, n, repeat, "move", "move_assignment_isolated", move_assign_times, move_assign_checksum, move_sources);
}

template<class Container>
static void run_all_for(const string& name, int n, int repeat, uint64_t seed) {
    bench_insert<Container>(name, n, repeat, seed);
    bench_erase<Container>(name, n, repeat, seed + 100);
    bench_query<Container>(name, n, repeat, seed + 200);
    bench_range<Container>(name, n, repeat, seed + 300);
    bench_iterator<Container>(name, n, repeat, seed + 400);
    bench_copy_move<Container>(name, n, repeat, seed + 500);
}

int main(int argc, char** argv) {
    int n = 200000;
    int repeat = 5;
    uint64_t seed = 20260426;
    string which = "both";

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        auto value = [&](const string& name) -> string {
            if (i + 1 >= argc) { cerr << "missing value for " << name << "\n"; exit(2); }
            return argv[++i];
        };
        if (arg == "--n") n = stoi(value(arg));
        else if (arg == "--repeat") repeat = stoi(value(arg));
        else if (arg == "--jobs") g_jobs = stoi(value(arg));
        else if (arg == "--seed") seed = stoull(value(arg));
        else if (arg == "--container") which = value(arg);
        else if (arg == "--help") {
            cout << "Usage: ./benchmark_single_op [--n N] [--repeat R] [--jobs J] [--seed S] [--container eset|stl|both]\n";
            cout << "Note: --jobs parallelizes independent repeats only. A single container run remains single-threaded.\n";
            return 0;
        } else {
            cerr << "unknown arg: " << arg << "\n";
            return 2;
        }
    }

    g_jobs = max(1, g_jobs);
    cout << "container,n,repeat,operation,pattern,mean_total_ns,sd_total_ns,avg_ns_per_logical_op,checksum\n";
    if (which == "eset" || which == "both") run_all_for<ESet<int>>("ESet", n, repeat, seed);
    if (which == "stl" || which == "both") run_all_for<std::set<int>>("std::set", n, repeat, seed);
    cerr << "global_sink=" << global_sink.load(memory_order_relaxed) << "\n";
    return 0;
}
