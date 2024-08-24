# compilation
out-of-tree= 插件代码目录在LLVM源码目录之外

```
# 提前export一个Clang_DIR变量，或者在这里打上绝对路径
cmake -DCT_Clang_INSTALL_DIR=$Clang_DIR ../xxxx/ # xxx你的插件代码路径
make
```
编译之后得到一个Libxxx.so文件
# 使用
两种插件加载方式
```
clang -fplugin=./xxx.so -Xclang -plugin-arg-CodeRefactor test.cpp
clang -cc1 -load ./xxx.so -plugin xxx test.cpp
```

# 关于同时生成可执行文件的说明

1. 在`class CodeRefactorAddPluginAction : public PluginASTAction`中实现`ActionType getActionType()`，即可自动执行plugin代码，例如
``` C
ActionType getActionType() override {
    return AddAfterMainAction;
}
```

2. plugin加载方式。[参考官网](https://clang.llvm.org/docs/ClangPlugins.html#clang-plugins)

```
clang -fplugin=./${plugin_file} ./${test_file} -o output
```

3. 关于`ActionType`的说明，参考[clang::PluginASTAction Class Reference](https://clang.llvm.org/doxygen/classclang_1_1PluginASTAction.html)

# 参考link
https://github.com/xiaoweiChen/LLVM-Techniques-Tips-and-Best-Practies

https://github.com/banach-space/clang-tutor

https://blog.quarkslab.com/implementing-a-custom-directive-handler-in-clang.html 
这个过时了但讲得还行。