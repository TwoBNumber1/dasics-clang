#include <stdio.h>

int A(char* src) {
    printf("A: %s\n", src);
    return 1;
}
int main() {
    #pragma bound ((src, src+len, DASICS_LIBCFG_R), (dest, dest+len, DASICS_LIBCFG_W))
    printf("execution.\n");
    A("New");
    return 0;
}
