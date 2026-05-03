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

enum class OpType {
    Emplace,
    Erase,
    Find,
    Range,
    LowerBound,
    UpperBound,
    Iterate,
    CopyAssign,
    MoveAssign
};

struct Operation {
    OpType type;
    int x;
    int y;
};

struct Timings {
    long long total_ns = 0;
    array<long long, 9> op_ns{};
    array<long long, 9> op_cnt{};
    uint64_t checksum = 0;
};

static const char* op_name(OpType t) {
    switch (t) {
        case OpType::Emplace: return "emplace";
        case OpType::Erase: return "erase";
        case OpType::Find: return "find";
        case OpType::Range: return "range";
        case OpType::LowerBound: return "lower_bound";
        case OpType::UpperBound: return "upper_bound";
        case OpType::Iterate: return "iterate_full";
        case OpType::CopyAssign: return "copy_assign";
        case OpType::MoveAssign: return "move_assign_from_copy";
    }
    return "unknown";
}

static int op_index(OpType t) { return static_cast<int>(t); }

template<class Container>
static size_t container_size(const Container& c) { return c.size(); }

template<class Container>
static size_t range_count(const Container& c, int l, int r) {
    if (r < l) return 0;
    if constexpr (std::is_same_v<Container, ESet<int>>) {
        return c.range(l, r);
    } else {
        return static_cast<size_t>(distance(c.lower_bound(l), c.upper_bound(r)));
    }
}

template<class Container>
static uint64_t full_iterate_checksum(Container& c) {
    uint64_t s = 0;
    for (auto it = c.begin(); it != c.end(); ++it) {
        s += static_cast<uint64_t>(*it) * 1315423911ull;
    }
    return s;
}

static vector<Operation> make_ops(int n_ops, uint64_t seed, int key_range) {
    vector<Operation> ops;
    ops.reserve(n_ops + 64);
    mt19937_64 rng(seed);
    uniform_int_distribution<int> key_dist(-key_range, key_range);

    auto push = [&](OpType t, int cnt) {
        for (int i = 0; i < cnt; ++i) {
            int x = key_dist(rng);
            int y = key_dist(rng);
            ops.push_back({t, x, y});
        }
    };

    int n_emplace = n_ops / 2;
    int n_erase = n_ops / 6;
    int n_find = n_ops / 9;
    int n_range = n_ops / 18;
    int n_copy = 25;
    int n_move = 25;
    int n_iter = max(10, n_ops / 2000);
    int used = n_emplace + n_erase + n_find + n_range + n_copy + n_move + n_iter;
    int remain = max(0, n_ops - used);
    int n_lb = remain / 2;
    int n_ub = remain - n_lb;

    push(OpType::Emplace, n_emplace);
    push(OpType::Erase, n_erase);
    push(OpType::Find, n_find);
    push(OpType::Range, n_range);
    push(OpType::LowerBound, n_lb);
    push(OpType::UpperBound, n_ub);
    push(OpType::Iterate, n_iter);
    push(OpType::CopyAssign, n_copy);
    push(OpType::MoveAssign, n_move);

    shuffle(ops.begin(), ops.end(), rng);
    return ops;
}

template<class Container>
static Timings run_ops(const vector<Operation>& ops, bool per_op_timing) {
    Container s;
    Timings result;

    auto total_begin = Clock::now();
    for (const auto& op : ops) {
        int idx = op_index(op.type);
        auto op_begin = per_op_timing ? Clock::now() : Clock::time_point{};

        switch (op.type) {
            case OpType::Emplace: {
                auto r = s.emplace(op.x);
                result.checksum += r.second;
                if (r.first != s.end()) result.checksum += static_cast<uint64_t>(*r.first);
                break;
            }
            case OpType::Erase: {
                result.checksum += s.erase(op.x);
                break;
            }
            case OpType::Find: {
                auto it = s.find(op.x);
                if (it != s.end()) result.checksum += static_cast<uint64_t>(*it);
                break;
            }
            case OpType::Range: {
                int l = min(op.x, op.y);
                int r = max(op.x, op.y);
                result.checksum += range_count(s, l, r);
                break;
            }
            case OpType::LowerBound: {
                auto it = s.lower_bound(op.x);
                if (it != s.end()) result.checksum += static_cast<uint64_t>(*it);
                break;
            }
            case OpType::UpperBound: {
                auto it = s.upper_bound(op.x);
                if (it != s.end()) result.checksum += static_cast<uint64_t>(*it);
                break;
            }
            case OpType::Iterate: {
                result.checksum += full_iterate_checksum(s);
                break;
            }
            case OpType::CopyAssign: {
                Container tmp;
                tmp = s;
                result.checksum += container_size(tmp);
                break;
            }
            case OpType::MoveAssign: {
                Container tmp;
                tmp = s;
                Container moved;
                moved = std::move(tmp);
                result.checksum += container_size(moved);
                break;
            }
        }

        if (per_op_timing) {
            auto op_end = Clock::now();
            result.op_ns[idx] += chrono::duration_cast<chrono::nanoseconds>(op_end - op_begin).count();
        }
        result.op_cnt[idx]++;
    }
    auto total_end = Clock::now();
    result.total_ns = chrono::duration_cast<chrono::nanoseconds>(total_end - total_begin).count();
    global_sink.fetch_add(result.checksum, memory_order_relaxed);
    return result;
}

template<class F>
static vector<Timings> run_repeats_parallel(int repeat, int jobs, F&& f) {
    vector<Timings> all(repeat);
    if (repeat <= 0) return all;
    jobs = max(1, min(jobs, repeat));
    if (jobs == 1) {
        for (int r = 0; r < repeat; ++r) all[r] = f(r);
        return all;
    }
    atomic<int> next{0};
    vector<thread> workers;
    workers.reserve(jobs);
    for (int j = 0; j < jobs; ++j) {
        workers.emplace_back([&] {
            while (true) {
                int r = next.fetch_add(1, memory_order_relaxed);
                if (r >= repeat) break;
                all[r] = f(r);
            }
        });
    }
    for (auto& th : workers) th.join();
    return all;
}

static void print_summary_csv(const string& container, int n_ops, int repeat, const vector<Timings>& all) {
    Timings sum;
    vector<long long> totals;
    for (const auto& t : all) {
        sum.total_ns += t.total_ns;
        sum.checksum += t.checksum;
        totals.push_back(t.total_ns);
        for (int i = 0; i < 9; ++i) {
            sum.op_ns[i] += t.op_ns[i];
            sum.op_cnt[i] += t.op_cnt[i];
        }
    }

    double mean_total = static_cast<double>(sum.total_ns) / max(1, repeat);
    double var = 0;
    for (auto x : totals) var += (x - mean_total) * (x - mean_total);
    var /= max(1, repeat - 1);
    double sd = sqrt(var);

    cout << "summary," << container << "," << n_ops << "," << repeat << ","
         << fixed << setprecision(2) << mean_total << "," << sd << "," << sum.checksum << "\n";

    for (int i = 0; i < 9; ++i) {
        long long cnt = sum.op_cnt[i];
        double avg = cnt ? static_cast<double>(sum.op_ns[i]) / cnt : 0.0;
        cout << "operation," << container << "," << op_name(static_cast<OpType>(i)) << ","
             << cnt << "," << fixed << setprecision(2) << avg << "," << sum.op_ns[i] << "\n";
    }
}

int main(int argc, char** argv) {
    int n_ops = 500000;
    int repeat = 5;
    int key_range = 1000000;
    int jobs = 1;
    uint64_t seed = 20260426;
    string which = "both";
    bool per_op_timing = true;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        auto value = [&](const string& name) -> string {
            if (i + 1 >= argc) { cerr << "missing value for " << name << "\n"; exit(2); }
            return argv[++i];
        };
        if (arg == "--n") n_ops = stoi(value(arg));
        else if (arg == "--repeat") repeat = stoi(value(arg));
        else if (arg == "--key-range") key_range = stoi(value(arg));
        else if (arg == "--jobs") jobs = stoi(value(arg));
        else if (arg == "--seed") seed = stoull(value(arg));
        else if (arg == "--container") which = value(arg);
        else if (arg == "--no-per-op-timing") per_op_timing = false;
        else if (arg == "--help") {
            cout << "Usage: ./benchmark_mixed [--n N] [--repeat R] [--jobs J] [--key-range K] [--seed S] [--container eset|stl|both] [--no-per-op-timing]\n";
            cout << "Note: --jobs parallelizes independent repeats only. A single container run remains single-threaded.\n";
            return 0;
        } else {
            cerr << "unknown arg: " << arg << "\n";
            return 2;
        }
    }

    cout << "kind,container,n_or_operation,repeat_or_count,mean_total_ns_or_avg_op_ns,sd_or_total_op_ns,checksum\n";

    auto ops = make_ops(n_ops, seed, key_range);

    if (which == "eset" || which == "both") {
        auto all = run_repeats_parallel(repeat, jobs, [&](int) {
            return run_ops<ESet<int>>(ops, per_op_timing);
        });
        print_summary_csv("ESet", n_ops, repeat, all);
    }

    if (which == "stl" || which == "both") {
        auto all = run_repeats_parallel(repeat, jobs, [&](int) {
            return run_ops<std::set<int>>(ops, per_op_timing);
        });
        print_summary_csv("std::set", n_ops, repeat, all);
    }

    cerr << "global_sink=" << global_sink.load(memory_order_relaxed) << "\n";
    return 0;
}
