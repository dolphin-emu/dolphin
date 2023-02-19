#ifndef RC_API_USER_H
#define RC_API_USER_H

#include "rc_api_request.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Login --- */

/**
 * API parameters for a login request.
 * If both password and api_token are provided, api_token will be ignored.
 */
typedef struct rc_api_login_request_t {
  /* The username of the player being logged in */
  const char* username;
  /* The API token from a previous login */
  const char* api_token;
  /* The player's password */
  const char* password;
}
rc_api_login_request_t;

/**
 * Response data for a login request.
 */
typedef struct rc_api_login_response_t {
  /* The case-corrected username of the player */
  const char* username;
  /* The API token to use for all future requests */
  const char* api_token;
  /* The current score of the player */
  unsigned score;
  /* The number of unread messages waiting for the player on the web site */
  unsigned num_unread_messages;
  /* The preferred name to display for the player */
  const char* display_name;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_login_response_t;

int rc_api_init_login_request(rc_api_request_t* request, const rc_api_login_request_t* api_params);
int rc_api_process_login_response(rc_api_login_response_t* response, const char* server_response);
void rc_api_destroy_login_response(rc_api_login_response_t* response);

/* --- Start Session --- */

/**
 * API parameters for a start session request.
 */
typedef struct rc_api_start_session_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  unsigned game_id;
}
rc_api_start_session_request_t;

/**
 * Response data for a start session request.
 */
typedef struct rc_api_start_session_response_t {
  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_start_session_response_t;

int rc_api_init_start_session_request(rc_api_request_t* request, const rc_api_start_session_request_t* api_params);
int rc_api_process_start_session_response(rc_api_start_session_response_t* response, const char* server_response);
void rc_api_destroy_start_session_response(rc_api_start_session_response_t* response);

/* --- Fetch User Unlocks --- */

/**
 * API parameters for a fetch user unlocks request.
 */
typedef struct rc_api_fetch_user_unlocks_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  unsigned game_id;
  /* Non-zero to fetch hardcore unlocks, 0 to fetch non-hardcore unlocks */
  int hardcore;
}
rc_api_fetch_user_unlocks_request_t;

/**
 * Response data for a fetch user unlocks request.
 */
typedef struct rc_api_fetch_user_unlocks_response_t {
  /* An array of achievement IDs previously unlocked by the user */
  unsigned* achievement_ids;
  /* The number of items in the achievement_ids array */
  unsigned num_achievement_ids;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_user_unlocks_response_t;

int rc_api_init_fetch_user_unlocks_request(rc_api_request_t* request, const rc_api_fetch_user_unlocks_request_t* api_params);
int rc_api_process_fetch_user_unlocks_response(rc_api_fetch_user_unlocks_response_t* response, const char* server_response);
void rc_api_destroy_fetch_user_unlocks_response(rc_api_fetch_user_unlocks_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* RC_API_H */
