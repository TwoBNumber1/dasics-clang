/usr/lib/llvm-18/bin/clang --target=riscv64-unknown-linux-gnu -march=rv64gc -fPIC -fplugin=./libCodeRefactor.so -fno-stack-protector \
 -O0 -g -static --sysroot=/opt/riscv/sysroot -I/home/zhangyulong/DASICS-case-study/LibDASICS/include  \
 -L/opt/riscv/sysroot/usr/lib -L/opt/riscv/sysroot/lib ./test/attack-case.c \
 -o ./attack-case /home/zhangyulong/DASICS-case-study/LibDASICS/build/LibDASICS.a -T/home/zhangyulong/DASICS-case-study/LibDASICS/ld.lds 
riscv64-unknown-linux-gnu-objdump -d ./attack-case > ./attack-case.txt
# /usr/lib/llvm-18/bin/clang --target=riscv64-unknown-linux-gnu -march=rv64gc -fPIC -fno-stack-protector -O0 -g -static --sysroot=/opt/riscv/sysroot -I/home/zhangyulong/DASICS-case-study/LibDASICS/include          source/attack-case.c -o build/attack-case /home/zhangyulong/DASICS-case-study/LibDASICS/build/LibDASICS.a -T/home/zhangyulong/DASICS-case-study/LibDASICS/ld.lds