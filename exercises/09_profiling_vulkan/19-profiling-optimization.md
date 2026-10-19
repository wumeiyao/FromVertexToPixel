# 第 19 章练习：可复现性能实验

对应教材：[第 19 章](../../chapters/19-profiling-optimization.md)  
完整源码：[chapter19.cpp](chapter19.cpp)

## 使用 Release 构建运行

~~~zsh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target chapter19
./build/exercises/chapter19
~~~

程序固定随机种子生成 20 万个点，比较两条产生完全相同可见数量的 CPU Culling 路径：基线计算 Distance，改动版比较 Distance Squared。两条路径先 Warm-up，再各采集 120 个样本。

Terminal 输出 Average、Median、P95、P99、Max 和平均加速比，原始数据写入：

~~~text
build/exercise_output/chapter19_profile.csv
~~~

这不是“平方距离永远更快”的结论，而是一份完整实验范例。不同 CPU、编译器和系统负载会得到不同结果，必须报告实际数字，包括没有收益的结果。

完成标准：只改变一个主要变量，先验证输出相同，再用相同条件的毫秒数据决定是否保留改动。
