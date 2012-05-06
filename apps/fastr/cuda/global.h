#ifndef __FINE_GRAINDED_GLOBAL_H__
#define __FINE_GRAINDED_GLOBAL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


#define MAX_LEN 800
#define CHAR_PER_LINE 50
#define PATH_END 0 //when PATH_END module 4, the result should be 3
#define COALESCED_OFFSET 32
#define BLOCK_NUM 1

typedef struct{
    char o;
    int length;
} SEQ_ALIGN;

#define ISTATE 3
#define DSTATE 1
#define MSTATE 2

#endif

