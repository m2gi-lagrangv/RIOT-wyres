#ifndef ___USERS___
#define ___USERS___

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

typedef struct user {
    char user_id[8];   //user id, unique         
    uint32_t message_id; // last message id, 2 users cant send same msg so unique
    struct user* next;
} user_t;
int add_user(user_t u);
size_t list_users(void);
int update_user(char uid[4], uint32_t message_id);
bool is_known_user(char uid[4]);
#endif