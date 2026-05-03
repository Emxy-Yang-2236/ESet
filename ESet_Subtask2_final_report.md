# ESet Subtask2 性能实验报告


## 1. 实验目标

本项目实现了一个基于红黑树的 `template<class Key, class Compare = std::less<Key>> class ESet`。Subtask2 的目标不是单纯说明程序“快”或“慢”，而是通过可复现实验比较 `ESet` 与 `std::set` 在相同操作序列下的性能差异，并解释差异来源。

本报告关注以下问题：

1. 在综合负载下，`ESet` 与 `std::set` 的总性能差异如何？
2. 在 `emplace`、`erase`、`find`、`lower_bound`、`upper_bound`、`range`、iterator、copy、move 等单独操作上，差异分别来自哪里？
3. `ESet::range(l,r)` 是否因为维护子树规模而获得结构性优势？
4. `ESet` 的基础操作慢于 STL 时，差距是否能用比较次数、分配次数、分配字节数等指标解释？

## 2. 实验环境


```text
CPU Model:        Intel(R) Core(TM) Ultra 9 275HX
CPU Cores:        24
RAM:              32 Gi
OS:               Ubuntu 24.04.3 LTS
Compiler:         gcc version 13.3.0
Compile Flags:    -std=c++17 -O2 -DNDEBUG
```

## 3. 测试方法

本次实验使用四类测试程序。

| 测试程序 | 作用 |
|---|---|
| `correctness_test.cpp` | 使用随机差分测试验证 `ESet<int>` 和 `std::set<int>` 行为一致 |
| `benchmark_mixed.cpp` | 按接近评测点的混合操作比例测试总性能和分操作耗时 |
| `benchmark_single_op.cpp` | 对每种操作分别做压力测试，观察不同输入模式下的性能 |
| `instrumented_benchmark.cpp` | 使用自定义比较器统计比较次数，并重载 `operator new` 统计分配次数和分配字节数 |

所有 benchmark 都尽量遵循以下原则：

1. `ESet` 和 `std::set` 使用完全相同的数据和操作序列。
2. 随机数种子固定，保证实验可复现。
3. 每组测试重复多次，输出平均值和标准差。
4. 使用 checksum 防止大部分查询结果被编译器优化掉。
5. `range` 测试中，`ESet::range(l,r)` 与 `std::distance(std::set::lower_bound(l), std::set::upper_bound(r))` 比较。该比较本质上也是对“是否维护 order-statistics 信息”的消融实验。

> RMK: 测试使用方法：
> ```
> cd Test_Program_Multithread_Ver
> chmod +x test_launch.sh
> sh test_launch.sh
> ```

## 4. 正确性测试

正确性测试输出如下：

```text
[correctness] total_ops=300000 jobs=16 key_range=600000 seed=123456789
PASS correctness_test
cases=16 total_ops=300000 final_size_sum=125436 checksum=18446744073694852000
```

测试共执行 300000 次随机操作，分为 16 个独立 case，key 范围为 `[0,600000)`，随机种子为 `123456789`。测试覆盖插入、删除、查找、上下界查询、区间查询、迭代器遍历、复制构造、复制赋值、移动构造和移动赋值等操作。结果为 PASS，说明后续性能测试建立在基本正确性之上。

其中 checksum 使用无符号整数累加，因此出现接近 `2^64` 的大数属于正常现象，不表示错误。

## 5. 综合性能测试

综合测试规模为 `N = 500000`，重复 `8` 次。

| Container | Mean total time | SD |
| --- | --- | --- |
| ESet | 10.324 s | 223.742 ms |
| std::set | 66.182 s | 404.502 ms |
| ESet / std::set | 0.156 | - |

从总时间看，`ESet` 的综合耗时约为 `std::set` 的 `0.156`，即表面上快约 `6.41` 倍。但是结论不能简单写成“ESet 整体快于 STL”，因为综合测试中的 `range` 操作对 `std::set` 极其不利。

###  综合测试中的分操作结果

| Operation | ESet avg | std::set avg | ESet / STL |
| --- | --- | --- | --- |
| emplace | 1190.55 ns | 771.46 ns | 1.543 |
| erase | 794.32 ns | 508.96 ns | 1.561 |
| find | 780.96 ns | 499.56 ns | 1.563 |
| lower_bound | 741.38 ns | 494.92 ns | 1.498 |
| upper_bound | 751.13 ns | 498.50 ns | 1.507 |
| range | 1474.08 ns | 2299998.89 ns | 0.001 |
| iterate_full | 15.123 ms | 5.631 ms | 2.686 |
| copy_assign | 117.506 ms | 11.027 ms | 10.656 |
| move_assign_from_copy | 123.477 ms | 11.833 ms | 10.435 |

可以看到，除了 `range` 操作外，`ESet` 在 `emplace`、`erase`、`find`、`lower_bound`、`upper_bound` 等基础操作上均慢于 `std::set`，比例大约在 `1.5` 到 `1.6` 之间。`iterate_full`、`copy_assign` 和 `move_assign_from_copy` 的差距更大。

`range` 操作是例外：`ESet` 平均约 `1474 ns/op`，而 `std::set` 平均约 `2.30 ms/op`。这个差距不是因为 `ESet` 的基础红黑树操作更快，而是因为 `ESet` 维护了子树规模，可以计算 rank；`std::set` 不提供 rank 接口，只能通过 `distance(lower_bound(l), upper_bound(r))` 线性遍历区间。

因此，综合测试的结论为：

> `ESet` 在基础红黑树操作上通常慢于 STL，但由于额外维护了子树大小，它在区间计数 `range(l,r)` 任务上具有结构性优势，从而拉低了综合测试总时间。

## 6. 单操作性能测试

单操作测试规模为 `N = 200000`，重复 `8` 次。

###  插入操作：`emplace`

| Pattern | ESet avg | std::set avg | ESet / STL |
| --- | --- | --- | --- |
| random_unique | 1187.06 ns | 646.85 ns | 1.84 |
| ascending | 1043.29 ns | 186.27 ns | 5.60 |
| descending | 1020.53 ns | 194.16 ns | 5.26 |
| many_duplicates | 283.74 ns | 90.91 ns | 3.12 |

`ESet` 在所有插入模式下都慢于 `std::set`。其中随机不重复插入约慢 `1.84` 倍，有序插入约慢 `5` 到 `6` 倍。由于两者均为红黑树结构，渐进复杂度同为 `O(log n)`，因此差距主要来自常数因素，包括智能指针管理、节点体积、父指针访问、维护子树规模、以及 STL 工程实现优化等。

###  删除操作：`erase`

| Pattern | ESet avg | std::set avg | ESet / STL |
| --- | --- | --- | --- |
| existing_random | 1324.47 ns | 333.73 ns | 3.97 |
| existing_ascending | 556.21 ns | 73.75 ns | 7.54 |
| existing_descending | 556.22 ns | 66.64 ns | 8.35 |

删除操作中，`ESet` 明显慢于 `std::set`。删除不仅包含查找，还包含节点替换、红黑树删除修复、颜色调整和子树 size 更新，因此会进一步放大实现常数差异。

本次测试中 `erase missing` 的 STL 时间极小，且部分类似 pure miss 查询的 checksum 为 0，可能存在编译器优化或测试构造过于特殊的问题。因此报告主体中不把 pure miss 作为核心结论。

###  查询操作：`find / lower_bound / upper_bound`

| Operation | Pattern | ESet avg | std::set avg | ESet / STL |
| --- | --- | --- | --- | --- |
| find | hit | 741.63 ns | 439.72 ns | 1.69 |
| lower_bound | hit | 903.73 ns | 472.23 ns | 1.91 |
| upper_bound | hit | 906.11 ns | 436.05 ns | 2.08 |
| find | half_hit_half_miss | 427.51 ns | 200.04 ns | 2.14 |
| lower_bound | half_hit_half_miss | 532.17 ns | 177.08 ns | 3.01 |
| upper_bound | half_hit_half_miss | 480.07 ns | 176.35 ns | 2.72 |

查询操作上，`ESet` 同样慢于 STL。`find(hit)` 约慢 `1.69` 倍，`lower_bound(hit)` 约慢 `1.91` 倍，`upper_bound(hit)` 约慢 `2.08` 倍。半命中半失败的情况下，`ESet` 也慢于 STL。

这说明在不涉及区间线性遍历时，`ESet` 的普通查找路径和单步访问常数都弱于 STL。

###  区间查询：`range(l,r)`

| Width | ESet avg | std::set avg | ESet / STL |
| --- | --- | --- | --- |
| width_1 | 1213.86 ns | 647.59 ns | 1.8744 |
| width_32 | 1546.75 ns | 2522.90 ns | 0.6131 |
| width_1024 | 1625.22 ns | 70296.06 ns | 0.0231 |
| width_n_over_2 | 1321.97 ns | 5634774.35 ns | 0.0002 |

`range` 是本次实验中最关键的现象。

当区间宽度为 `1` 时，`std::set` 更快，说明它的基础查找常数更小；但是随着区间宽度增大，`std::set` 的耗时快速上升，而 `ESet` 的耗时基本维持在 `1.2us` 到 `1.6us` 左右。

这是因为：

```cpp
ESet::range(l, r)                 // 利用子树 size，近似 O(log n)
std::distance(lb, ub)             // 双向迭代器线性移动，O(k)
```

其中 `k` 是区间内元素数量。随着 `k` 增大，`std::set` 的公开接口实现会迅速变慢，而 `ESet` 的 rank 计算不需要遍历区间中每个元素。

因此，`range` 实验支持以下 claim：

> `ESet` 在大区间计数上显著优于 `std::set`，优势来自额外维护子树规模，而不是来自普通红黑树操作的常数更低。

###  迭代器遍历

| Pattern | ESet avg | std::set avg | ESet / STL |
| --- | --- | --- | --- |
| forward_full_traversal | 164.63 ns | 77.73 ns | 2.12 |
| reverse_full_traversal | 472.23 ns | 73.66 ns | 6.41 |

`ESet` 的迭代器遍历慢于 STL。正向完整遍历约慢 `2.12` 倍，反向完整遍历约慢 `6.41` 倍。可能原因是 `ESet` 的父指针使用 `weak_ptr`，访问父节点时需要 `lock()` 产生临时智能指针；反向遍历中前驱查找对父指针依赖更强，因此差距更明显。

###  复制与移动

复制测试：

| Pattern | ESet avg | std::set avg | ESet / STL |
| --- | --- | --- | --- |
| copy_assignment | 1203.93 ns | 193.23 ns | 6.23 |
| copy_constructor | 1180.39 ns | 194.00 ns | 6.08 |

移动测试：

| Pattern | ESet avg | std::set avg | ESet / STL |
| --- | --- | --- | --- |
| move_assignment_isolated | 20884604.38 ns | 3358715.16 ns | 6.22 |
| move_constructor_isolated | 19114193.73 ns | 4550984.38 ns | 4.20 |

复制操作中，`ESet` 约慢 `6` 倍。结合源码设计，原因很可能是 `ESet` 的复制通过遍历原集合并逐个插入实现，因此会重新比较、重新旋转并重新维护 size；而 STL 的复制构造通常可以直接复制已有树结构，避免大量比较。

移动测试中的 `move_constructor_isolated` 和 `move_assignment_isolated` 包含了为每次 trial 准备源对象的成本，因此不能简单解释为“move 本身慢”。更稳妥的说法是：在包含源对象构造的移动场景中，`ESet` 仍明显慢于 `std::set`，主要差距很可能仍来自源对象构造和节点管理。

## 7. Instrumented Benchmark：比较次数与分配统计

为了进一步解释性能差异，实验使用自定义比较器统计比较次数，并通过重载 `operator new` 统计分配次数和分配字节数。测试规模为 `N = 200000`。

| Scenario | ESet time | std::set time | ESet cmp | STL cmp | ESet bytes | STL bytes | Time ratio |
| --- | --- | --- | --- | --- | --- | --- | --- |
| insert_random_unique | 118.061 ms | 26.909 ms | 4,959,475 | 3,706,033 | 20,800,104 | 8,000,000 | 4.39 |
| find_hit | 75.671 ms | 41.347 ms | 5,202,956 | 3,798,298 | 0 | 0 | 1.83 |
| copy_constructor | 159.158 ms | 16.812 ms | 5,944,713 | 0 | 20,800,104 | 8,000,000 | 9.47 |
| copy_then_erase_all_random | 254.840 ms | 32.849 ms | 10,646,843 | 5,154,842 | 20,800,104 | 8,000,000 | 7.76 |
| range_width_1 | 134.048 ms | 65.625 ms | 9,000,997 | 7,197,122 | 0 | 0 | 2.04 |
| range_width_n_over_2 | 122.165 ms | 317.167 s | 8,143,511 | 7,196,205 | 0 | 0 | 0.00 |

###  插入慢不只是因为比较次数更多

在 `insert_random_unique` 中，`ESet` 的时间约为 STL 的 `4.39` 倍，但比较次数只约为 STL 的 `1.34` 倍。与此同时，`ESet` 的分配字节数约为 STL 的 `2.60` 倍。

这说明插入性能差距不能只用比较次数解释。更可能的瓶颈包括：

1. `shared_ptr / weak_ptr` 的引用计数和 `lock()` 成本；
2. 节点体积更大，导致 cache locality 较差；
3. 每个节点额外维护 `size`、颜色、父子指针等信息；
4. `optional<Key>` 或类似设计带来的额外访问成本；
5. STL 的节点布局、内存分配和旋转修复代码经过了更充分优化。

### 查询慢同样不完全来自比较次数

在 `find_hit` 中，`ESet` 的时间约为 STL 的 `1.83` 倍，比较次数约为 STL 的 `1.37` 倍。比较次数的增加能解释一部分差距，但不能完全解释所有时间差。这说明每一次节点访问、父子指针跳转和比较器调用之外的常数成本也很重要。

###  复制构造差距来自实现策略

在 `copy_constructor` 中，`std::set` 的比较次数为 `0`，而 `ESet` 的比较次数为 `5,944,713`。这说明 STL 的复制构造没有通过“逐个插入”来重建树，而 `ESet` 的复制更接近“遍历 + 插入”的策略。

因此，`ESet` 的复制构造明显慢于 STL，是实现策略差异导致的，而不仅仅是红黑树常数问题。

###  大区间 range 的优势不是来自比较次数

在 `range_width_n_over_2` 中，`ESet` 和 STL 的比较次数处于同一数量级，但时间差距极大。`std::set` 的总时间达到 `317.167 s`，而 `ESet` 仅为 `122.165 ms`。

这说明 STL 在大区间 range 中的主要开销不是边界查找比较，而是 `std::distance` 在线性遍历区间元素时产生的大量 iterator++ 操作。

## 8. Profiling 补充实验

除了基于 `std::chrono` 的操作级计时和 `instrumented_benchmark` 的比较/分配统计外，本实验还尝试使用 `gperftools` 对程序进行补充分析。


### 8.1 gperftools CPU Profile

由于 `perf` 硬件事件在 WSL2 下不可用，进一步尝试使用 gperftools 的 CPU profiler 进行采样分析。测试对象仍然是 `benchmark_mixed`，规模为 `N = 500000`，`repeat = 1`，`jobs = 1`，分别对 `ESet` 和 `std::set` 单独运行。

示例命令如下：

```bash
g++ -std=c++17 -O2 -DNDEBUG -g -fno-omit-frame-pointer -rdynamic \
  benchmark_mixed.cpp -o benchmark_mixed

CPUPROFILE=gperf_out/mixed_eset.prof \
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so \
./benchmark_mixed --n 500000 --repeat 1 --jobs 1 --container eset \
> gperf_out/mixed_eset.csv

CPUPROFILE=gperf_out/mixed_stl.prof \
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libprofiler.so \
./benchmark_mixed --n 500000 --repeat 1 --jobs 1 --container stl \
> gperf_out/mixed_stl.csv

google-pprof --text ./benchmark_mixed gperf_out/mixed_eset.prof \
> gperf_out/mixed_eset_cpu.txt

google-pprof --text ./benchmark_mixed gperf_out/mixed_stl.prof \
> gperf_out/mixed_stl_cpu.txt
```

####  `std::set` 的热点集中在迭代器线性前进

`std::set` 的 CPU profile 共得到 `2091` 个采样，其中 `std::_Rb_tree_increment` 占 `1949` 个样本，即 `93.2%`。

| Symbol | Samples | Percentage |
|---|---:|---:|
| `std::_Rb_tree_increment@@GLIBCXX_3.4` | 1949 / 2091 | 93.2% |
| `_int_malloc` | 10 / 2091 | 0.5% |
| `std::_Rb_tree_insert_and_rebalance` | 1 / 2091 | 0.0% |

这个结果非常关键：`std::_Rb_tree_increment` 正是 `std::set` 迭代器 `++` 的底层前进操作。结合前面的 `range` 实验，`std::set` 的 `range(l,r)` 是通过

```cpp
std::distance(st.lower_bound(l), st.upper_bound(r))
```

实现的，而 `std::set` 迭代器是双向迭代器，`std::distance` 需要不断执行 iterator++。因此，当区间较大时，大量时间消耗在 `std::_Rb_tree_increment` 上。这直接支持前文结论：综合测试中 `std::set` 变慢的主要原因不是边界查找比较，而是 `range` 中对区间元素的线性遍历。

这也解释了为什么在 `range_width_n_over_2` 中，`std::set` 的比较次数和 `ESet` 处于同一数量级，但总时间却达到 `317.167 s`：比较器统计没有覆盖 iterator++ 的线性移动成本，而 CPU profile 明确显示 `std::set` 的运行时间主要集中在红黑树迭代器前进函数中。

####  `ESet` 的 profile 结果与限制

`ESet` 的 CPU profile 共得到 `532` 个采样。与 `std::set` 不同，`ESet` 的 profile 没有出现一个占比接近 `std::_Rb_tree_increment` 的单一热点；采样分布在多个未完全符号化的地址上，并包含 `_int_malloc`、`_int_free`、`operator new` 等内存管理相关函数。

| Symbol / Address | Samples | Percentage |
|---|---:|---:|
| unresolved address `0x...f20e` | 28 / 532 | 5.3% |
| unresolved address `0x...3ea6` | 25 / 532 | 4.7% |
| unresolved address `0x...3eeb` | 25 / 532 | 4.7% |
| `_int_malloc` | 24 / 532 | 4.5% |
| `operator new` cumulative | 29 / 532 | 5.5% |
| `_int_free` | 4 / 532 | 0.8% |

这个结果只能作为辅助证据，不能过度解读。原因有三点：

1. `ESet` 在综合测试中运行时间远短于 `std::set`，因此 CPU profile 的总采样数只有 `532`，样本数相对较少。
2. 输出中许多热点没有被完整解析成源码函数名，只显示为地址，因此不能精确断言某个地址一定对应 `shared_ptr::lock()`、旋转函数或 `range` 相关函数。
3. 由于混合测试同时包含多种操作，ESet 的时间分布本来就可能分散在插入、删除、查找、复制、迭代器和 range 等多个实现细节上。

因此，本报告只将 ESet 的 gperftools 结果作为“没有单一线性 iterator++ 热点、存在一定内存分配相关开销”的辅助现象；关于 ESet 慢于 STL 的主要解释，仍然以 `instrumented_benchmark` 中的比较次数和分配字节数为准。

###  Profiling 小结

`perf` 硬件事件在 WSL2 环境下不可用，因此不能用来支撑 cache miss、branch miss 或 CPU cycles 相关 claim。gperftools CPU profile 则提供了一个有价值的补充：

1. `std::set` 在综合测试中的时间高度集中于 `std::_Rb_tree_increment`，说明其总耗时被 `range` 中的线性 iterator++ 主导；
2. `ESet` 没有类似的单一线性遍历热点，符合 `range` 使用子树规模而非遍历区间元素的设计；
3. ESet profile 的函数名解析不充分，因此不把它作为智能指针、节点体积等具体成本的直接证据；这些结论主要由 `instrumented_benchmark` 的分配字节数、比较次数和操作级计时支撑。

## 9. 总体结论

根据 correctness、综合性能测试、单操作测试、instrumented benchmark 以及 profiling 结果，可以得到以下结论。

###  正确性

`ESet` 通过了 300000 次随机差分 correctness 测试，行为与 `std::set` 基本一致。因此后续性能比较建立在正确性基本可信的前提上。

### 普通红黑树操作：`ESet` 慢于 STL

在 `emplace`、`erase`、`find`、`lower_bound`、`upper_bound` 等基础操作上，`ESet` 普遍慢于 `std::set`。在综合测试中，这些基础操作大约慢 `1.5` 到 `1.6` 倍；在单操作压力测试中，部分场景差距更大。

这说明虽然二者渐进复杂度同为 `O(log n)`，但 STL 的工程实现常数明显更小。根据 `instrumented_benchmark`，`ESet` 在随机插入中比较次数约为 STL 的 `1.34` 倍，但总时间约为 `4.39` 倍，分配字节数约为 `2.60` 倍。因此性能差距不能只由比较次数解释，节点结构和内存管理成本是重要来源。

###  `range(l,r)`：`ESet` 具有结构性优势

`range` 是本实验中最明显的反例。对于大区间，`ESet` 远快于 `std::set`。单操作测试中，当区间宽度为 `n/2` 时，`ESet` 平均约 `1321.97 ns/op`，而 `std::set` 平均约 `5634774.35 ns/op`。

原因是 `ESet` 维护了子树规模，可以通过 rank 思想计算区间元素个数；而 `std::set` 的公开接口没有 rank 信息，只能通过 `distance(lower_bound, upper_bound)` 线性遍历区间。gperftools 对 `std::set` 的采样结果也显示，`93.2%` 的样本集中在 `std::_Rb_tree_increment`，与这个解释一致。

因此，综合测试中 `ESet` 总时间低于 `std::set`，不应解释为“ESet 的红黑树整体实现比 STL 更快”，而应解释为：

> `ESet` 在普通基础操作上常数更大，但由于额外维护子树大小，它在区间计数任务上具有 `std::set` 公开接口不具备的结构性优势。

###  复制操作：`ESet` 的实现策略明显劣于 STL

复制构造和复制赋值是 `ESet` 的明显弱项。`instrumented_benchmark` 显示，`copy_constructor` 中 `std::set` 的比较次数为 `0`，而 `ESet` 的比较次数为 `5,944,713`。这说明 STL 的复制构造没有通过逐个插入来重建树，而 `ESet` 更接近“遍历原树并逐个插入新树”的策略。

因此，`ESet` 的复制慢主要不是红黑树理论问题，而是实现策略问题。后续如果直接递归复制整棵树并维护 parent、color 和 size，有望显著改善复制性能。

###  总体分析

理论上 `ESet` 和 `std::set` 都是红黑树，但性能差距来自以下几个方面：

1. **是否维护额外信息不同**：`ESet` 维护子树规模，所以 `range(l,r)` 更快；STL 不提供 rank 信息，只能线性统计区间长度。
2. **节点和内存管理不同**：`ESet` 使用智能指针和更大的节点结构，分配字节数明显高于 STL，普通操作常数更大。
3. **复制策略不同**：`ESet` 复制时逐个插入，STL 复制树结构的优化更充分。
4. **迭代器实现不同**：`ESet` 的正向和反向迭代器遍历均慢于 STL，反向遍历差距尤其明显。
5. **工程优化程度不同**：STL 的红黑树实现经过长期优化，节点布局、内存分配、迭代器前进和旋转修复等细节都更成熟。

因此，本实验最终结论为：

> `ESet` 不是整体意义上比 STL 更快。它在基础红黑树操作上的工程常数较大，但在 `range(l,r)` 这种区间计数任务上，因为额外维护子树规模，具有明显的结构性优势。性能差距的来源不是单一因素，而是数据结构功能差异、节点设计、内存管理和复制策略共同作用的结果。

## 10. 后续优化方向

基于当前实验，`ESet` 可以考虑以下优化方向：

1. 用裸指针或自定义内存池替代 `shared_ptr / weak_ptr`，减少引用计数和 `lock()` 成本。
2. 将 NIL 节点和真实节点的存储分离，减少 `optional<Key>` 或类似额外包装带来的访问成本。
3. 优化复制构造：不要通过逐个插入重建树，而是直接递归复制原树结构，并同步维护 parent、color 和 size。

## 11. 实验局限性

本实验仍然存在一些局限：

1. `perf` 硬件事件在 WSL2 中不可用，因此无法直接统计 cache miss、branch miss 和 instruction count。
2. gperftools 对 ESet 的 CPU profile 未能完整解析模板函数名，因此只能作为辅助证据。
3. `range` 比较中，`ESet::range` 与 `std::set + distance` 并不是完全同一功能实现方式，而是对“是否维护 order-statistics 信息”的比较。因此该结果应解释为功能设计优势，而不是基础红黑树常数优势。
4. 部分 pure miss 场景的 checksum 为 0，可能存在编译器优化影响，因此报告主体没有把这些场景作为核心证据。

## 12. 参考与工具

本实验主要使用以下工具：

1. `std::chrono::steady_clock`：操作级计时；
2. 随机差分测试：验证 `ESet` 与 `std::set` 行为一致；
3. 自定义比较器：统计比较次数；
4. 重载 `operator new`：统计分配次数和分配字节数；
5. `perf stat`：尝试统计硬件事件，但由于 WSL2 不支持相关事件，未作为主要证据；
6. gperftools CPU profiler：用于观察混合测试中的采样热点。
