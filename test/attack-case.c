#include <stdio.h>
#include <udasics.h>
#include <uattr.h>
#include <string.h>
#include <stdlib.h>

void rop_target();
void ATTR_UFREEZONE_TEXT Malicious(void * unsed, const void * src);
void ATTR_UFREEZONE_TEXT homebrew_memcpy(void * dst, const void * src, size_t length);
void ATTR_ULIB_TEXT just_call_back();
#define OLD_BP_PTR   __builtin_frame_address(0)
#define RET_ADDR_PTR ((void **) OLD_BP_PTR - 1)

uint64_t stack_buffer_handler;
uint64_t stack_handler;

static char data_secret[32] = "success. Secret data leaked.\n";


int main(int argc, char * argv[])
{
    csr_write(0x880, 0);

    register_udasics(0);
    // Save secret data
    char stack_secret_data[32];
    strcpy(stack_secret_data, data_secret);

    // Buffer send to Malicious
    char stack_buffer[1024];

    printf("> [Begin] Call Malicious\n");

    // stack_buffer_handler =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, \
    //                                                     (uint64_t)stack_buffer, \
    //                                                     (uint64_t)stack_buffer + 1024 -1 );
    /* 栈指针sp的默认处理 */
    // uint64_t sp;
    // asm volatile ("mv %0, sp" : "=r"(sp));
    // stack_handler = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, \
    //                                                     sp - 0x2000, \
    //                                                     sp);
    // #pragma bound (stack_buffer, 1024, DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R)
    #pragma untrusted_call
    Malicious(NULL, stack_buffer);
    // lib_call(&Malicious, (uint64_t)stack_buffer);

    printf("> [End] Back to Main\n");

    // dasics_libcfg_free(stack_buffer_handler);
    // dasics_libcfg_free(stack_handler);
    
    unregister_udasics();
    return 0;
}

void
rop_target()
{
    printf("\x1b[31m Attack success.\nROP function reached. \x1b[0m\n");
    exit(0);
}

void ATTR_UFREEZONE_TEXT 
Malicious(void * unsed, const void * src)
{
    // Local buffer on stack
    size_t length = 1024;
    char private_buffer[length + 32 + 16];

    // Copy main's stack_buffer, stack_secret_data, ret_address
    homebrew_memcpy(private_buffer, src, length + 32 + 16);

    char * secret = &private_buffer[length];

    uint64_t * ret_addr = (uint64_t *)(&private_buffer[length + 32 + 8]);

    dasics_umaincall(Umaincall_PRINT, \
                        " - - - - - - Malicious Call - - - - - - - -  \n");

    dasics_umaincall(Umaincall_PRINT, \
                        "\x1b[31m Steal Secret data: %s \x1b[0m\n", secret);


    dasics_umaincall(Umaincall_PRINT, \
                        "\x1b[31m Ret address in stack: 0x%lx, ret_addr: 0x%lx \x1b[0m\n", \
                        (uint64_t)&((char *)src)[length + 32 + 8], \
                        (uint64_t)*ret_addr);

    dasics_umaincall(Umaincall_PRINT, \
                        "x1b[31m Try to tamper return address of main to Rop: 0x%lx \x1b[0m\n", \
                     (uint64_t)&rop_target + 8
                     );    

    // Try to change ret address and do Rop attack
    ((uint64_t* )src)[(length + 32 + 8) / 8] = (uint64_t)&rop_target + 8;

    dasics_umaincall(Umaincall_PRINT, \
                        " - - - - - - Malicious Call End- - - - - - - -  \n");

    return;

}



void ATTR_UFREEZONE_TEXT
homebrew_memcpy(void * dst, const void * src, size_t length)
{
    char * d, * s;

    d = (char *) dst;
    s = (char *) src;

    while (length--) {
        *d++ = *s++;
    }
}

void ATTR_ULIB_TEXT __attribute__((unused))
just_call_back() 
{
    return;
}