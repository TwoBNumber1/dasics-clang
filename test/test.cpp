#include <stdio.h>

int A() {
    return 1;
}
int main() {
    #pragma bound ((src, src+len, DASICS_LIBCFG_R), (dest, dest+len, DASICS_LIBCFG_W))
    printf("execution.\n");
    A();
    return 0;
}
