#ifndef __MSG__
#define __MSG__

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
typedef struct {
    char uid[4];
    uint32_t message_id;
    char group[4];
    char target_user[4];
    uint8_t ttl;
    char message[32];
} chat_message_t;
#endif