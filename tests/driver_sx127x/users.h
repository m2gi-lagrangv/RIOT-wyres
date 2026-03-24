#ifndef ___USERS__
#define __USERS__

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

typedef struct user {
    uint32_t user_id;   //user id, unique         
    uint32_t message_id; // last message id, 2 users cant send same msg so unique
    struct user* next;
} user_t;
int add_user(user_t u);
size_t list_users(void);
#endif