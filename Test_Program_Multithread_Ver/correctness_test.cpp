#include <algorithm>
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

struct CaseResult {
    size_t final_size = 0;
    uint64_t checksum = 0;
};

static vector<int> to_vector_eset(const ESet<int>& s) {
    vector<int> v;
    v.reserve(s.size());
    for (auto it = s.begin(); it != s.end(); ++it) v.push_back(*it);
    return v;
}

static vector<int> to_vector_std(const std::set<int>& s) {
    return vector<int>(s.begin(), s.end());
}

static void require_true(bool cond, const string& msg) {
    if (!cond) {
        cerr << "[FAIL] " << msg << "\n";
        std::exit(1);
    }
}

static void check_same_content(const ESet<int>& my, const std::set<int>& st, const string& where) {
    auto a = to_vector_eset(my);
    auto b = to_vector_std(st);
    if (a != b) {
        cerr << "[FAIL] content mismatch at " << where << "\n";
        cerr << "ESet size=" << a.size() << ", std::set size=" << b.size() << "\n";
        cerr << "First different index: ";
        size_t m = min(a.size(), b.size());
        size_t idx = 0;
        while (idx < m && a[idx] == b[idx]) ++idx;
        cerr << idx << "\n";
        if (idx < a.size()) cerr << "ESet[idx]=" << a[idx] << "\n";
        if (idx < b.size()) cerr << "std[idx]=" << b[idx] << "\n";
        std::exit(1);
    }
    require_true(my.size() == st.size(), where + ": size() mismatch");
}

static size_t std_range_count(const std::set<int>& st, int l, int r) {
    if (r < l) return 0;
    return static_cast<size_t>(distance(st.lower_bound(l), st.upper_bound(r)));
}

static void check_iterator_contract(ESet<int>& my, const std::set<int>& st, uint64_t& checksum, const string& where) {
    check_same_content(my, st, where + ": forward traversal");

    auto e = my.end();
    ++e;
    require_true(e == my.end(), where + ": ++end() should stay end()");

    bool threw = false;
    try {
        checksum += static_cast<uint64_t>(*my.end());
    } catch (const std::exception&) {
        threw = true;
    }
    require_true(threw, where + ": *end() should throw");

    if (!st.empty()) {
        auto b = my.begin();
        int first = *b;
        --b;
        require_true(*b == first, where + ": --begin() should stay begin()");

        vector<int> rev_my;
        auto it = my.end();
        while (it != my.begin()) {
            --it;
            rev_my.push_back(*it);
        }
        vector<int> rev_std(st.rbegin(), st.rend());
        require_true(rev_my == rev_std, where + ": reverse traversal mismatch");
    }
}

static void check_copy_and_move(ESet<int>& my, const std::set<int>& st, mt19937_64& rng, int key_range, const string& where) {
    ESet<int> cp(my);
    check_same_content(cp, st, where + ": copy constructor");

    ESet<int> assigned;
    assigned = my;
    check_same_content(assigned, st, where + ": copy assignment");

    if (!st.empty()) {
        int victim = *st.begin();
        cp.erase(victim);
        require_true(cp.size() + 1 == my.size(), where + ": copy independence after erase");
        check_same_content(my, st, where + ": original after copy modified");
    }

    uniform_int_distribution<int> dist(-key_range, key_range);
    int extra = dist(rng);
    assigned.emplace(extra);
    check_same_content(my, st, where + ": original after assigned copy modified");

    ESet<int> tmp(my);
    ESet<int> moved(std::move(tmp));
    check_same_content(moved, st, where + ": move constructor");

    ESet<int> tmp2(my);
    ESet<int> moved2;
    moved2 = std::move(tmp2);
    check_same_content(moved2, st, where + ": move assignment");
}

static CaseResult run_case(int ops, int key_range, uint64_t seed, int case_id) {
    uint64_t checksum = 0;
    ESet<int> my;
    std::set<int> st;
    mt19937_64 rng(seed);
    uniform_int_distribution<int> key_dist(-key_range, key_range);
    uniform_int_distribution<int> op_dist(0, 999);

    for (int i = 1; i <= ops; ++i) {
        int op = op_dist(rng);
        int x = key_dist(rng);
        int y = key_dist(rng);
        string pos = "case " + to_string(case_id) + ", op " + to_string(i);

        if (op < 420) {
            auto r1 = my.emplace(x);
            auto r2 = st.emplace(x);
            require_true(r1.second == r2.second, "emplace bool mismatch at " + pos);
            require_true(r1.first != my.end(), "emplace iterator is end at " + pos);
            require_true(*r1.first == *r2.first, "emplace iterator value mismatch at " + pos);
        } else if (op < 610) {
            size_t a = my.erase(x);
            size_t b = st.erase(x);
            require_true(a == b, "erase return mismatch at " + pos);
        } else if (op < 760) {
            auto a = my.find(x);
            auto b = st.find(x);
            if (b == st.end()) {
                require_true(a == my.end(), "find should return end at " + pos);
            } else {
                require_true(a != my.end(), "find should not return end at " + pos);
                require_true(*a == *b, "find value mismatch at " + pos);
                checksum += static_cast<uint64_t>(*a);
            }
        } else if (op < 840) {
            auto a = my.lower_bound(x);
            auto b = st.lower_bound(x);
            if (b == st.end()) {
                require_true(a == my.end(), "lower_bound should return end at " + pos);
            } else {
                require_true(a != my.end(), "lower_bound should not return end at " + pos);
                require_true(*a == *b, "lower_bound value mismatch at " + pos);
                checksum += static_cast<uint64_t>(*a);
            }
        } else if (op < 920) {
            auto a = my.upper_bound(x);
            auto b = st.upper_bound(x);
            if (b == st.end()) {
                require_true(a == my.end(), "upper_bound should return end at " + pos);
            } else {
                require_true(a != my.end(), "upper_bound should not return end at " + pos);
                require_true(*a == *b, "upper_bound value mismatch at " + pos);
                checksum += static_cast<uint64_t>(*a);
            }
        } else if (op < 970) {
            int l = min(x, y);
            int r = max(x, y);
            size_t a = my.range(l, r);
            size_t b = std_range_count(st, l, r);
            require_true(a == b, "range mismatch at " + pos);
            checksum += a;
        } else if (op < 990) {
            check_iterator_contract(my, st, checksum, pos);
        } else {
            check_copy_and_move(my, st, rng, key_range, pos);
        }

        if ((i % 10000) == 0) {
            check_same_content(my, st, "periodic check at " + pos);
        }
    }

    check_same_content(my, st, "final case " + to_string(case_id));
    check_iterator_contract(my, st, checksum, "final case " + to_string(case_id));
    check_copy_and_move(my, st, rng, key_range, "final case " + to_string(case_id));
    return CaseResult{my.size(), checksum};
}

int main(int argc, char** argv) {
    int ops = 300000;
    int key_range = 600000;
    int jobs = 1;
    uint64_t seed = 123456789;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        auto need_value = [&](const string& name) -> string {
            if (i + 1 >= argc) {
                cerr << "missing value for " << name << "\n";
                exit(2);
            }
            return argv[++i];
        };
        if (arg == "--ops") ops = stoi(need_value(arg));
        else if (arg == "--key-range") key_range = stoi(need_value(arg));
        else if (arg == "--jobs") jobs = stoi(need_value(arg));
        else if (arg == "--seed") seed = stoull(need_value(arg));
        else if (arg == "--help") {
            cout << "Usage: ./correctness_test [--ops N] [--jobs J] [--key-range K] [--seed S]\n";
            cout << "Note: --jobs runs independent random cases in parallel; it does not share one ESet across threads.\n";
            return 0;
        } else {
            cerr << "unknown arg: " << arg << "\n";
            return 2;
        }
    }

    jobs = max(1, min(jobs, max(1, ops)));
    int base_ops = ops / jobs;
    int rem = ops % jobs;
    cerr << "[correctness] total_ops=" << ops << " jobs=" << jobs
         << " key_range=" << key_range << " seed=" << seed << "\n";

    vector<CaseResult> results(jobs);
    vector<thread> workers;
    workers.reserve(jobs);
    for (int j = 0; j < jobs; ++j) {
        int this_ops = base_ops + (j < rem ? 1 : 0);
        uint64_t this_seed = seed + 1000003ull * static_cast<uint64_t>(j);
        workers.emplace_back([&, j, this_ops, this_seed] {
            results[j] = run_case(this_ops, key_range, this_seed, j);
        });
    }
    for (auto& th : workers) th.join();

    uint64_t checksum = 0;
    size_t final_size_sum = 0;
    for (auto& r : results) {
        checksum += r.checksum;
        final_size_sum += r.final_size;
    }

    cout << "PASS correctness_test\n";
    cout << "cases=" << jobs << " total_ops=" << ops
         << " final_size_sum=" << final_size_sum << " checksum=" << checksum << "\n";
    return 0;
}
