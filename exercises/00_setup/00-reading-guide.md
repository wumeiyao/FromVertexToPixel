# 第 0 章练习：工程怎样变成程序

对应教材：[第 0 章](../../chapters/00-reading-guide.md)  
完整源码：[chapter00.cpp](chapter00.cpp)

## 构建运行

~~~zsh
cmake --build build --target chapter00
./build/exercises/chapter00
~~~

程序输出 `Hello, renderer!`，随后打印：

~~~text
source -> compiler -> object file -> linker -> executable
~~~

这条线就是 VS Code 点击 Build 时真正发生的事。VS Code 是操作入口；CMake 生成构建规则；Ninja 调用编译器和链接器；最后得到可以由操作系统启动的 `chapter00` 文件。

## 阅读和修改

1. 从 `main()` 开始，确认程序入口和返回值。
2. 修改第一行文字并重新 Build，观察可执行文件更新时间。
3. 故意删掉一个分号，阅读编译器给出的文件名和行号。
4. 恢复后再运行，确认“编译成功”和“程序运行”是两个动作。

完成标准：能不用 `tasks.json`，只靠根 CMake 配置构建和运行这个目标。
