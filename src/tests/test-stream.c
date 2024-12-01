#include <stdio.h>

#include "../util/stream.h"

int main() {
    printf("Test stream start\n");
    uint8_t buff[] = {1, 2, 3, 4, 5 , 6, 7, 8};
    unsigned size = 8;
    struct Stream sIn = constructStream(&buff, size);
    uint8_t res = 0;
    int i = 0;
    for (i=0; i < size; i++){
        BEReadU8(&sIn,&res);
        printf("res: %d\n", res);
        if (res != buff[i])
            return 1;
    }
    return 0;
}