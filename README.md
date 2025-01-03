# 面向DASICS的编译器实现
## 编程接口
### #pragma untrusted_call
  ```C
  #pragma untrusted_call  //需要标注在调用点处
  strcpy(src,dest);
  ```
  标注在第三方库函数调用前，该条预处理指令做三个事情：
  1. 替换函数调用指令为dasicscall;
  2. 栈保护边界设置;
  3. 利用DFA Pass的分析结果自动给实参填充bound。
  V0（仅分析用户程序部分）

### #pragma bound((param1_base,param1_offset,permission),...)
  ```C
  #pragma bound((src,src+len,DASICS_LIBCFG_R))  //需要标注在调用点处
  strcpy(src,dest);
  ```
  标注在第三方库函数调用前，每个括号内需要3个数据来确定一个Bound，起始地址，偏移以及读写权限，多个参数的Bound设置用逗号分隔。
  该条预处理指令做三个事情：
  1. 替换函数调用指令为dasicscall;
  2. 栈保护边界设置;
  3. 根据参数指定设置对应参数的Bound，如果参数指定不全，利用DFA Pass的分析结果自动给未指定实参填充bound。
   V0（仅分析用户程序部分）

### #pragma marked_functions
  ```C 
  #pragma marked_functions
  #include <stdio.h> // 仅识别紧跟在pragma后的第一个头文件
  ```
  标注在头文件之前，识别头文件中的函数声明，在当前程序中所有来自该头文件的函数都做untrusted_call处理。

### #pragma marked_functions begin/end
  标注多个untrusted头文件
  ```C 
  #pragma marked_functions begin
  #include <stdio.h> // 仅识别begin-end范围内的头文件
  #include <ctype.h>
  ...
  #pragma marked_functions end
  ```
## 边界分析
完整的边界分析主要由用户程序指向分析（分析出被调用的数据对象）及库函数分析（库函数对数据对象的内存访问行为）构成
目前仅实现针对用户程序的稀疏数据流分析。

# Compilation
out-of-tree= 代码在LLVM源码目录之外，以插件形式使用
```Shell
# 提前export一个Clang_DIR变量，或者在这里打上绝对路径
cmake -DCT_Clang_INSTALL_DIR=$Clang_DIR ../xxxx/ # 插件源码路径
make
```
需要进入PragmHandler和DFAPass文件夹下分别编译出LibCodeRefactor.so （处理Pragma）和 LibAnalysisPass.so（分析并回填Bound）

# 使用
两种插件加载方式
```Shell
clang -fplugin=./xxx.so -Xclang -plugin-arg-CodeRefactor test.cpp
clang -cc1 -load ./xxx.so -plugin xxx test.cpp
```

# RISCV 交叉编译命令
 clang --target=riscv64-unknown-linux-gnu -march=rv64gc -fPIC -Xclang -load -Xclang ./libCodeRefactor.so -fno-stack-protector -O0 -g -static --sysroot=/opt/riscv/sysroot -I/yourpath/DASICS-case-study/LibDASICS/include  -L/opt/riscv/sysroot/usr/lib -L/opt/riscv/sysroot/lib source/attack-case.c -o build/attack-case /yourpath/DASICS-case-study/LibDASICS/build/LibDASICS.a -T/yourpath/DASICS-case-study/LibDASICS/ld.lds 

# qemu
Under `qemu-dasics` folder, run `run_qemu.sh` to start qemu.

# 说明
+ #pragma untrusted_call 必须紧跟函数调用

+ Modify CMakeLists.txt to change the version of dasics compiler

```C
set(_CodeRefactor_SOURCE
  CodeRefactor_case.cpp // for no pragma bound test
  CodeRefactor_recompile.cpp // for pragma bound test
)
```

# 参考link
https://github.com/xiaoweiChen/LLVM-Techniques-Tips-and-Best-Practies

https://github.com/banach-space/clang-tutor

https://blog.quarkslab.com/implementing-a-custom-directive-handler-in-clang.html 
这个过时了但讲得还行。