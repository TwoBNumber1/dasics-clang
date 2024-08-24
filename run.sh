# clang -cc1 -load ./libCodeRefactor.so -plugin CodeRefactor ./test.cpp
# clang -Xclang -load -Xclang ./libCodeRefactor.so -Xclang -plugin -Xclang CodeRefactor ./test.cpp
clang -fplugin=./libCodeRefactor.so test.cpp -o output
# clang -fplugin=./libCodeRefactor.so -fplugin-arg-CodeRefactor-param1=value1 -fplugin-arg-CodeRefactor-param2=value2 test.cpp -o output