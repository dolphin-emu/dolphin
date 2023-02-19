#include "rc_api_info.h"
#include "rc_api_common.h"

#include "rc_runtime_types.h"

#include <stdlib.h>
#include <string.h>

/* --- Fetch Achievement Info --- */

int rc_api_init_fetch_achievement_info_request(rc_api_request_t* request, const rc_api_fetch_achievement_info_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request);

  if (api_params->achievement_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "achievementwondata", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "a", api_params->achievement_id);

    if (api_params->friends_only)
      rc_url_builder_append_unum_param(&builder, "f", 1);
    if (api_params->first_entry > 1)
      rc_url_builder_append_unum_param(&builder, "o", api_params->first_entry - 1); /* number of entries to skip */
    rc_url_builder_append_unum_param(&builder, "c", api_params->count);

    request->post_data = rc_url_builder_finalize(&builder);
  }

  return builder.result;
}

int rc_api_process_fetch_achievement_info_response(rc_api_fetch_achievement_info_response_t* response, const char* server_response) {
  rc_api_achievement_awarded_entry_t* entry;
  rc_json_field_t iterator;
  unsigned timet;
  int result;

  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"AchievementID"},
    {"Response"}
    /* unused fields
    {"Offset"},
    {"Count"},
    {"FriendsOnly"},
     * unused fields */
  };

  rc_json_field_t response_fields[] = {
    {"NumEarned"},
    {"TotalPlayers"},
    {"GameID"},
    {"RecentWinner"} /* array */
  };

  rc_json_field_t entry_fields[] = {
    {"User"},
    {"DateAwarded"}
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  if (!rc_json_get_required_unum(&response->id, &response->response, &fields[2], "AchievementID"))
      return RC_MISSING_VALUE;
  if (!rc_json_get_required_object(response_fields, sizeof(response_fields) / sizeof(response_fields[0]), &response->response, &fields[3], "Response"))
    return RC_MISSING_VALUE;

  if (!rc_json_get_required_unum(&response->num_awarded, &response->response, &response_fields[0], "NumEarned"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->num_players, &response->response, &response_fields[1], "TotalPlayers"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->game_id, &response->response, &response_fields[2], "GameID"))
    return RC_MISSING_VALUE;

  if (!rc_json_get_required_array(&response->num_recently_awarded, &iterator, &response->response, &response_fields[3], "RecentWinner"))
    return RC_MISSING_VALUE;

  if (response->num_recently_awarded) {
    response->recently_awarded = (rc_api_achievement_awarded_entry_t*)rc_buf_alloc(&response->response.buffer, response->num_recently_awarded * sizeof(rc_api_achievement_awarded_entry_t));
    if (!response->recently_awarded)
      return RC_OUT_OF_MEMORY;

    entry = response->recently_awarded;
    while (rc_json_get_array_entry_object(entry_fields, sizeof(entry_fields) / sizeof(entry_fields[0]), &iterator)) {
      if (!rc_json_get_required_string(&entry->username, &response->response, &entry_fields[0], "User"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_unum(&timet, &response->response, &entry_fields[1], "DateAwarded"))
        return RC_MISSING_VALUE;
      entry->awarded = (time_t)timet;

      ++entry;
    }
  }

  return RC_OK;
}

void rc_api_destroy_fetch_achievement_info_response(rc_api_fetch_achievement_info_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}

/* --- Fetch Leaderboard Info --- */

int rc_api_init_fetch_leaderboard_info_request(rc_api_request_t* request, const rc_api_fetch_leaderboard_info_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request);

  if (api_params->leaderboard_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "lbinfo");
  rc_url_builder_append_unum_param(&builder, "i", api_params->leaderboard_id);

  if (api_params->username)
    rc_url_builder_append_str_param(&builder, "u", api_params->username);
  else if (api_params->first_entry > 1)
    rc_url_builder_append_unum_param(&builder, "o", api_params->first_entry - 1); /* number of entries to skip */

  rc_url_builder_append_unum_param(&builder, "c", api_params->count);
  request->post_data = rc_url_builder_finalize(&builder);

  return builder.result;
}

int rc_api_process_fetch_leaderboard_info_response(rc_api_fetch_leaderboard_info_response_t* response, const char* server_response) {
  rc_api_lboard_info_entry_t* entry;
  rc_json_field_t iterator;
  unsigned timet;
  int result;
  size_t len;
  char format[16];

  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"LeaderboardData"}
  };

  rc_json_field_t leaderboarddata_fields[] = {
    {"LBID"},
    {"LBFormat"},
    {"LowerIsBetter"},
    {"LBTitle"},
    {"LBDesc"},
    {"LBMem"},
    {"GameID"},
    {"LBAuthor"},
    {"LBCreated"},
    {"LBUpdated"},
    {"Entries"} /* array */
    /* unused fields
    {"GameTitle"},
    {"ConsoleID"},
    {"ConsoleName"},
    {"ForumTopicID"},
    {"GameIcon"},
     * unused fields */
  };

  rc_json_field_t entry_fields[] = {
    {"User"},
    {"Rank"},
    {"Index"},
    {"Score"},
    {"DateSubmitted"}
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  if (!rc_json_get_required_object(leaderboarddata_fields, sizeof(leaderboarddata_fields) / sizeof(leaderboarddata_fields[0]), &response->response, &fields[2], "LeaderboardData"))
    return RC_MISSING_VALUE;

  if (!rc_json_get_required_unum(&response->id, &response->response, &leaderboarddata_fields[0], "LBID"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_num(&response->lower_is_better, &response->response, &leaderboarddata_fields[2], "LowerIsBetter"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->title, &response->response, &leaderboarddata_fields[3], "LBTitle"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->description, &response->response, &leaderboarddata_fields[4], "LBDesc"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->definition, &response->response, &leaderboarddata_fields[5], "LBMem"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->game_id, &response->response, &leaderboarddata_fields[6], "GameID"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->author, &response->response, &leaderboarddata_fields[7], "LBAuthor"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_datetime(&response->created, &response->response, &leaderboarddata_fields[8], "LBCreated"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_datetime(&response->updated, &response->response, &leaderboarddata_fields[9], "LBUpdated"))
    return RC_MISSING_VALUE;

  if (!leaderboarddata_fields[1].value_end)
    return RC_MISSING_VALUE;
  len = leaderboarddata_fields[1].value_end - leaderboarddata_fields[1].value_start - 2;
  if (len < sizeof(format) - 1) {
    memcpy(format, leaderboarddata_fields[1].value_start + 1, len);
    format[len] = '\0';
    response->format = rc_parse_format(format);
  }
  else {
    response->format = RC_FORMAT_VALUE;
  }

  if (!rc_json_get_required_array(&response->num_entries, &iterator, &response->response, &leaderboarddata_fields[10], "Entries"))
    return RC_MISSING_VALUE;

  if (response->num_entries) {
    response->entries = (rc_api_lboard_info_entry_t*)rc_buf_alloc(&response->response.buffer, response->num_entries * sizeof(rc_api_lboard_info_entry_t));
    if (!response->entries)
      return RC_OUT_OF_MEMORY;

    entry = response->entries;
    while (rc_json_get_array_entry_object(entry_fields, sizeof(entry_fields) / sizeof(entry_fields[0]), &iterator)) {
      if (!rc_json_get_required_string(&entry->username, &response->response, &entry_fields[0], "User"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_unum(&entry->rank, &response->response, &entry_fields[1], "Rank"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_unum(&entry->index, &response->response, &entry_fields[2], "Index"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_num(&entry->score, &response->response, &entry_fields[3], "Score"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_unum(&timet, &response->response, &entry_fields[4], "DateSubmitted"))
        return RC_MISSING_VALUE;
      entry->submitted = (time_t)timet;

      ++entry;
    }
  }

  return RC_OK;
}

void rc_api_destroy_fetch_leaderboard_info_response(rc_api_fetch_leaderboard_info_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}

/* --- Fetch Games List --- */

int rc_api_init_fetch_games_list_request(rc_api_request_t* request, const rc_api_fetch_games_list_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request);

  if (api_params->console_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "gameslist");
  rc_url_builder_append_unum_param(&builder, "c", api_params->console_id);

  request->post_data = rc_url_builder_finalize(&builder);

  return builder.result;
}

int rc_api_process_fetch_games_list_response(rc_api_fetch_games_list_response_t* response, const char* server_response) {
  rc_api_game_list_entry_t* entry;
  rc_json_object_field_iterator_t iterator;
  int result;
  char* end;

  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"Response"}
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  if (!fields[2].value_start) {
    /* call rc_json_get_required_object to generate the error message */
    rc_json_get_required_object(NULL, 0, &response->response, &fields[2], "Response");
    return RC_MISSING_VALUE;
  }

  response->num_entries = fields[2].array_size;
  rc_buf_reserve(&response->response.buffer, response->num_entries * (32 + sizeof(rc_api_game_list_entry_t)));

  response->entries = (rc_api_game_list_entry_t*)rc_buf_alloc(&response->response.buffer, response->num_entries * sizeof(rc_api_game_list_entry_t));
  if (!response->entries)
    return RC_OUT_OF_MEMORY;

  memset(&iterator, 0, sizeof(iterator));
  iterator.json = fields[2].value_start;

  entry = response->entries;
  while (rc_json_get_next_object_field(&iterator)) {
    entry->id = strtol(iterator.field.name, &end, 10);

    iterator.field.name = "";
    if (!rc_json_get_string(&entry->name, &response->response.buffer, &iterator.field, ""))
      return RC_MISSING_VALUE;

    ++entry;
  }

  return RC_OK;
}

void rc_api_destroy_fetch_games_list_response(rc_api_fetch_games_list_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}
