#include "users.h"
user_t* all_users;

int add_user(user_t u){
    user_t* new_user = malloc(sizeof(user_t));
    new_user->user_id = u.user_id;
    new_user->message_id = u.message_id;
    new_user->next = NULL;
    
    if (all_users == NULL) {
        all_users = new_user;
    } else {
        user_t* current = all_users;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_user;
    }
    return 0;
}

size_t list_users(void){
    user_t* user= &all_users[0];
    size_t count=0;
    while(user!=NULL){
        printf("user id: %li, last message id: %li \n",user->user_id,user->message_id);
        user= user->next;
        count++;
    }
    return count;
}
