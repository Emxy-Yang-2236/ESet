#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <random>
#include <cassert>
#include "src.hpp" // 确保你的头文件路径正确

// 暴力统计区间内元素个数，作为参考答案
template <typename T>
size_t brute_force_range(const std::set<T>& std_s, T l, T r) {
    if (l > r) return 0;
    auto it_l = std_s.lower_bound(l);
    auto it_r = std_s.upper_bound(r);
    return std::distance(it_l, it_r);
}

void run_test(int operations_count) {
    ESet<int> my_set;
    std::set<int> std_set;

    std::mt19937 rng(1337); // 固定随机种子以便复现
    std::uniform_int_distribution<int> op_dist(0, 3); // 操作类型
    std::uniform_int_distribution<int> val_dist(0, 1000); // 值的范围

    std::cout << "开始测试 " << operations_count << " 个随机操作..." << std::endl;

    for (int i = 0; i < operations_count; ++i) {
        int op = op_dist(rng);
        int val = val_dist(rng);

        if (op == 0) { // 插入
            my_set.insert(val);
            std_set.insert(val);
        }
        else if (op == 1) { // 删除
            my_set.erase(val);
            std_set.erase(val);
        }
        else if (op == 2 || op == 3) { // Range 查询测试
            int l = val_dist(rng);
            int r = val_dist(rng);
            if (l > r) std::swap(l, r);

            size_t expected = brute_force_range(std_set, l, r);
            size_t actual = my_set.range(l, r);

            if (expected != actual) {
                std::cerr << "错误！操作步骤: " << i << std::endl;
                std::cerr << "区间: [" << l << ", " << r << "]" << std::endl;
                std::cerr << "期望值: " << expected << ", 实际值: " << actual << std::endl;
                std::cerr << "当前 ESet 大小: " << my_set.size() << ", std::set 大小: " << std_set.size() << std::endl;
                exit(1);
            }
        }

        // 每 100 步额外验证一次 size 属性
        if (i % 100 == 0) {
            assert(my_set.size() == std_set.size());
        }
    }

    std::cout << "测试通过！" << std::endl;
}

int main() {
    try {
        // 先进行小规模高强度测试
        run_test(1000);
        // 再进行大规模测试
        run_test(50000);
    } catch (const std::exception& e) {
        std::cerr << "捕获到异常: " << e.what() << std::endl;
    }
    return 0;
}