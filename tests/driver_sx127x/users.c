#include "users.h"
#include <string.h>
user_t *all_users = NULL;

int add_user(user_t u)
{
    user_t *new_user = malloc(sizeof(user_t));
    strncpy(new_user->user_id, u.user_id, 4);
    new_user->user_id[3] = '\0'; 
    new_user->message_id = u.message_id;
    new_user->next = NULL;

    if (all_users == NULL)
    {
        all_users = new_user;
    }
    else
    {
        user_t *current = all_users;
        while (current != NULL)
        {
            if (strcmp(current->user_id, u.user_id) == 0)
            {
                current->message_id = u.message_id;
                free(new_user);   // pas besoin, on repart
                return 0;
            }
            if (current->next == NULL) break;
            current = current->next;
        }
        current->next = new_user;
    }
    return 0;
}

size_t list_users(void)
{
    user_t *user = all_users;
    size_t count = 0;

    if (user == NULL)
    {
        puts("(no known users)");
        return 0;
    }
    while (user != NULL)
    {
        printf("user id: %s, last message id: %" PRIu32 "\n",
               user->user_id, user->message_id);
        user = user->next;
        count++;
    }
    return count;
}

int update_user(char uid[4], uint32_t message_id)
{
    user_t u = { .message_id = message_id, .next = NULL };
    strcpy(u.user_id, uid);
    return add_user(u);
}

bool is_known_user(char uid[4]) {
    user_t *users = all_users;
    while (users != NULL) {
        if (strcmp(users->user_id, uid) == 0) {
            return true;
        }
        users = users->next;
    }
    return false;
}