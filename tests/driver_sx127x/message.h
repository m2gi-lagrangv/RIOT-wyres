#ifndef __MSG__
#define __MSG__

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
typedef struct {
    uint32_t uid;
    uint32_t target;
    uint32_t message_id;
    char message[32];
} chat_message_t;
#endif