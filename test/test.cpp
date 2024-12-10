#include <stdio.h>

int A(void) {
    return 1;
}
int main() {
    #pragma bound (stack_buffer, 1024, DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R)
    #pragma untrusted_call
    A();
    printf("execution.\n");
    return 0;
}
