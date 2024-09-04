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

# RISCV 交叉编译命令
 clang --target=riscv64-unknown-linux-gnu -march=rv64gc -fPIC -Xclang -load -Xclang ./libCodeRefactor.so -fno-stack-protector -O0 -g -static --sysroot=/opt/riscv/sysroot -I/yourpath/DASICS-case-study/LibDASICS/include  -L/opt/riscv/sysroot/usr/lib -L/opt/riscv/sysroot/lib source/attack-case.c -o build/attack-case /yourpath/DASICS-case-study/LibDASICS/build/LibDASICS.a -T/yourpath/DASICS-case-study/LibDASICS/ld.lds 

# qemu
...

# 参考link
https://github.com/xiaoweiChen/LLVM-Techniques-Tips-and-Best-Practies

https://github.com/banach-space/clang-tutor

https://blog.quarkslab.com/implementing-a-custom-directive-handler-in-clang.html 
这个过时了但讲得还行。