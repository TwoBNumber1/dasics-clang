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

# 参考link
https://github.com/xiaoweiChen/LLVM-Techniques-Tips-and-Best-Practies

https://github.com/banach-space/clang-tutor

https://blog.quarkslab.com/implementing-a-custom-directive-handler-in-clang.html 
这个过时了但讲得还行。