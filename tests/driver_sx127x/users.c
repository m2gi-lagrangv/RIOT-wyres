#include "users.h"
user_t *all_users = NULL;

int add_user(user_t u)
{
    user_t *new_user = malloc(sizeof(user_t));
    new_user->user_id = u.user_id;
    new_user->message_id = u.message_id;
    new_user->next = NULL;

    if (all_users == NULL)
    {
        all_users = new_user;
    }
    else
    {
        user_t *current = all_users;
        while (current->next != NULL)
        {
            current = current->next;
            if (current->user_id == u.user_id)
            {
                current->message_id = u.message_id;
                return 0;
            }
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
        printf("user id: %" PRIu32 ", last message id: %" PRIu32 "\n",
               user->user_id, user->message_id);
        user = user->next;
        count++;
    }
    return count;
}

int update_user(uint32_t uid, uint32_t message_id)
{
    user_t u = { .user_id = uid, .message_id = message_id, .next = NULL };
    return add_user(u);
}

bool is_known_user(uint32_t uid) {
    user_t *users = all_users;
    while (users != NULL) {
        if (users->user_id == uid) {
            return true;
        }
        users = users->next;
    }
    return false;
}