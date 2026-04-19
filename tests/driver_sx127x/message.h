#ifndef __MSG__
#define __MSG__

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
typedef struct {
    uint32_t uid;
    uint32_t message_id;
    uint32_t group;
    uint32_t target_user;
    char message[32];
} chat_message_t;
#endif