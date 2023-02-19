#ifndef RC_API_RUNTIME_H
#define RC_API_RUNTIME_H

#include "rc_api_request.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Fetch Image --- */

/**
 * API parameters for a fetch image request.
 * NOTE: fetch image server response is the raw image data. There is no rc_api_process_fetch_image_response function.
 */
typedef struct rc_api_fetch_image_request_t {
  /* The name of the image to fetch */
  const char* image_name;
  /* The type of image to fetch */
  int image_type;
}
rc_api_fetch_image_request_t;

#define RC_IMAGE_TYPE_GAME 1
#define RC_IMAGE_TYPE_ACHIEVEMENT 2
#define RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED 3
#define RC_IMAGE_TYPE_USER 4

int rc_api_init_fetch_image_request(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params);

/* --- Resolve Hash --- */

/**
 * API parameters for a resolve hash request.
 */
typedef struct rc_api_resolve_hash_request_t {
  /* Unused - hash lookup does not require credentials */
  const char* username;
  /* Unused - hash lookup does not require credentials */
  const char* api_token;
  /* The generated hash of the game to be identified */
  const char* game_hash;
}
rc_api_resolve_hash_request_t;

/**
 * Response data for a resolve hash request.
 */
typedef struct rc_api_resolve_hash_response_t {
  /* The unique identifier of the game, 0 if no match was found */
  unsigned game_id;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_resolve_hash_response_t;

int rc_api_init_resolve_hash_request(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params);
int rc_api_process_resolve_hash_response(rc_api_resolve_hash_response_t* response, const char* server_response);
void rc_api_destroy_resolve_hash_response(rc_api_resolve_hash_response_t* response);

/* --- Fetch Game Data --- */

/**
 * API parameters for a fetch game data request.
 */
typedef struct rc_api_fetch_game_data_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  unsigned game_id;
}
rc_api_fetch_game_data_request_t;

/* A leaderboard definition */
typedef struct rc_api_leaderboard_definition_t {
  /* The unique identifier of the leaderboard */
  unsigned id;
  /* The format to pass to rc_format_value to format the leaderboard value */
  int format;
  /* The title of the leaderboard */
  const char* title;
  /* The description of the leaderboard */
  const char* description;
  /* The definition of the leaderboard to be passed to rc_runtime_activate_lboard */
  const char* definition;
  /* Non-zero if lower values are better for this leaderboard */
  int lower_is_better;
  /* Non-zero if the leaderboard should not be displayed in a list of leaderboards */
  int hidden;
}
rc_api_leaderboard_definition_t;

/* An achievement definition */
typedef struct rc_api_achievement_definition_t {
  /* The unique identifier of the achievement */
  unsigned id;
  /* The number of points the achievement is worth */
  unsigned points;
  /* The achievement category (core, unofficial) */
  unsigned category;
  /* The title of the achievement */
  const char* title;
  /* The dscription of the achievement */
  const char* description;
  /* The definition of the achievement to be passed to rc_runtime_activate_achievement */
  const char* definition;
  /* The author of the achievment */
  const char* author;
  /* The image name for the achievement badge */
  const char* badge_name;
  /* When the achievement was first uploaded to the server */
  time_t created;
  /* When the achievement was last modified on the server */
  time_t updated;
}
rc_api_achievement_definition_t;

#define RC_ACHIEVEMENT_CATEGORY_CORE 3
#define RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL 5

/**
 * Response data for a fetch game data request.
 */
typedef struct rc_api_fetch_game_data_response_t {
  /* The unique identifier of the game */
  unsigned id;
  /* The console associated to the game */
  unsigned console_id;
  /* The title of the game */
  const char* title;
  /* The image name for the game badge */
  const char* image_name;
  /* The rich presence script for the game to be passed to rc_runtime_activate_richpresence */
  const char* rich_presence_script;

  /* An array of achievements for the game */
  rc_api_achievement_definition_t* achievements;
  /* The number of items in the achievements array */
  unsigned num_achievements;

  /* An array of leaderboards for the game */
  rc_api_leaderboard_definition_t* leaderboards;
  /* The number of items in the leaderboards array */
  unsigned num_leaderboards;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_game_data_response_t;

int rc_api_init_fetch_game_data_request(rc_api_request_t* request, const rc_api_fetch_game_data_request_t* api_params);
int rc_api_process_fetch_game_data_response(rc_api_fetch_game_data_response_t* response, const char* server_response);
void rc_api_destroy_fetch_game_data_response(rc_api_fetch_game_data_response_t* response);

/* --- Ping --- */

/**
 * API parameters for a ping request.
 */
typedef struct rc_api_ping_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  unsigned game_id;
  /* (optional) The current rich presence evaluation for the user */
  const char* rich_presence;
}
rc_api_ping_request_t;

/**
 * Response data for a ping request.
 */
typedef struct rc_api_ping_response_t {
  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_ping_response_t;

int rc_api_init_ping_request(rc_api_request_t* request, const rc_api_ping_request_t* api_params);
int rc_api_process_ping_response(rc_api_ping_response_t* response, const char* server_response);
void rc_api_destroy_ping_response(rc_api_ping_response_t* response);

/* --- Award Achievement --- */

/**
 * API parameters for an award achievement request.
 */
typedef struct rc_api_award_achievement_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the achievement */
  unsigned achievement_id;
  /* Non-zero if the achievement was earned in hardcore */
  int hardcore;
  /* The hash associated to the game being played */
  const char* game_hash;
}
rc_api_award_achievement_request_t;

/**
 * Response data for an award achievement request.
 */
typedef struct rc_api_award_achievement_response_t {
  /* The unique identifier of the achievement that was awarded */
  unsigned awarded_achievement_id;
  /* The updated player score */
  unsigned new_player_score;
  /* The number of achievements the user has not yet unlocked for this game
   * (in hardcore/non-hardcore per hardcore flag in request) */
  unsigned achievements_remaining;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_award_achievement_response_t;

int rc_api_init_award_achievement_request(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params);
int rc_api_process_award_achievement_response(rc_api_award_achievement_response_t* response, const char* server_response);
void rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response);

/* --- Submit Leaderboard Entry --- */

/**
 * API parameters for a submit lboard entry request.
 */
typedef struct rc_api_submit_lboard_entry_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the leaderboard */
  unsigned leaderboard_id;
  /* The value being submitted */
  int score;
  /* The hash associated to the game being played */
  const char* game_hash;
}
rc_api_submit_lboard_entry_request_t;

/* A leaderboard entry */
typedef struct rc_api_lboard_entry_t {
  /* The user associated to the entry */
  const char* username;
  /* The rank of the entry */
  unsigned rank;
  /* The value of the entry */
  int score;
}
rc_api_lboard_entry_t;

/**
 * Response data for a submit lboard entry request.
 */
typedef struct rc_api_submit_lboard_entry_response_t {
  /* The value that was submitted */
  int submitted_score;
  /* The player's best submitted value */
  int best_score;
  /* The player's new rank within the leaderboard */
  unsigned new_rank;
  /* The total number of entries in the leaderboard */
  unsigned num_entries;

  /* An array of the top entries for the leaderboard */
  rc_api_lboard_entry_t* top_entries;
  /* The number of items in the top_entries array */
  unsigned num_top_entries;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_submit_lboard_entry_response_t;

int rc_api_init_submit_lboard_entry_request(rc_api_request_t* request, const rc_api_submit_lboard_entry_request_t* api_params);
int rc_api_process_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response, const char* server_response);
void rc_api_destroy_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* RC_API_RUNTIME_H */
