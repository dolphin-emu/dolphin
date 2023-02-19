#ifndef RC_URL_H
#define RC_URL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int rc_url_award_cheevo(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned cheevo_id, int hardcore, const char* game_hash);

int rc_url_submit_lboard(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned lboard_id, int value);

int rc_url_get_lboard_entries(char* buffer, size_t size, unsigned lboard_id, unsigned first_index, unsigned count);
int rc_url_get_lboard_entries_near_user(char* buffer, size_t size, unsigned lboard_id, const char* user_name, unsigned count);

int rc_url_get_gameid(char* buffer, size_t size, const char* hash);

int rc_url_get_patch(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid);

int rc_url_get_badge_image(char* buffer, size_t size, const char* badge_name);

int rc_url_login_with_password(char* buffer, size_t size, const char* user_name, const char* password);

int rc_url_login_with_token(char* buffer, size_t size, const char* user_name, const char* login_token);

int rc_url_get_unlock_list(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid, int hardcore);

int rc_url_post_playing(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid);

int rc_url_ping(char* url_buffer, size_t url_buffer_size, char* post_buffer, size_t post_buffer_size,
                const char* user_name, const char* login_token, unsigned gameid, const char* rich_presence);

#ifdef __cplusplus
}
#endif

#endif /* RC_URL_H */
