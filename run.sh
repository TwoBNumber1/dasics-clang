# clang -cc1 -load ./libCodeRefactor.so -plugin CodeRefactor ./test/test.cpp
# clang -Xclang -load -Xclang ./libCodeRefactor.so -Xclang -plugin -Xclang CodeRefactor ./test/test.cpp
/usr/lib/llvm-18/bin/clang -fplugin=./libCodeRefactor.so ./test/test.cpp -o output
# clang -fplugin=./libCodeRefactor.so -fplugin-arg-CodeRefactor-param1=value1 -fplugin-arg-CodeRefactor-param2=value2 ./test/test.cpp -o output