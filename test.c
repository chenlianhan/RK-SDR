#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void printf2(uint16_t n) {
    uint16_t i = 0;
    for(i = 0; i < 16; i++) {
        if(n & (0x8000) >> i) {
            printf("1");
        }else {
            printf("0");
        }
    }
    printf("\n");
}

int main(int argc, const char * argv[]) {
    uint16_t i = 35;
    uint16_t j = 520;
    uint16_t k = 1080;
    printf2(i);
    printf2(j);
    printf2(k);
    return 0;
}
