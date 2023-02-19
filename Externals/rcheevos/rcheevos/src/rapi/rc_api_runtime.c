#include "rc_api_runtime.h"
#include "rc_api_common.h"

#include "rc_runtime.h"
#include "rc_runtime_types.h"
#include "../rcheevos/rc_compat.h"
#include "../rhash/md5.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* --- Resolve Hash --- */

int rc_api_init_resolve_hash_request(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request);

  if (!api_params->game_hash || !*api_params->game_hash)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "gameid");
  rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);
  request->post_data = rc_url_builder_finalize(&builder);

  return builder.result;
}

int rc_api_process_resolve_hash_response(rc_api_resolve_hash_response_t* response, const char* server_response) {
  int result;
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"GameID"},
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  rc_json_get_required_unum(&response->game_id, &response->response, &fields[2], "GameID");
  return RC_OK;
}

void rc_api_destroy_resolve_hash_response(rc_api_resolve_hash_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}

/* --- Fetch Game Data --- */

int rc_api_init_fetch_game_data_request(rc_api_request_t* request, const rc_api_fetch_game_data_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request);

  if (api_params->game_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "patch", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
    request->post_data = rc_url_builder_finalize(&builder);
  }

  return builder.result;
}

int rc_api_process_fetch_game_data_response(rc_api_fetch_game_data_response_t* response, const char* server_response) {
  rc_api_achievement_definition_t* achievement;
  rc_api_leaderboard_definition_t* leaderboard;
  rc_json_field_t iterator;
  const char* str;
  const char* last_author = "";
  size_t last_author_len = 0;
  size_t len;
  unsigned timet;
  int result;
  char format[16];

  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"PatchData"} /* nested object */
  };

  rc_json_field_t patchdata_fields[] = {
    {"ID"},
    {"Title"},
    {"ConsoleID"},
    {"ImageIcon"},
    {"RichPresencePatch"},
    {"Achievements"}, /* array */
    {"Leaderboards"} /* array */
    /* unused fields
    {"ForumTopicID"},
    {"Flags"},
     * unused fields */
  };

  rc_json_field_t achievement_fields[] = {
    {"ID"},
    {"Title"},
    {"Description"},
    {"Flags"},
    {"Points"},
    {"MemAddr"},
    {"Author"},
    {"BadgeName"},
    {"Created"},
    {"Modified"}
  };

  rc_json_field_t leaderboard_fields[] = {
    {"ID"},
    {"Title"},
    {"Description"},
    {"Mem"},
    {"Format"},
    {"LowerIsBetter"},
    {"Hidden"}
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_object(patchdata_fields, sizeof(patchdata_fields) / sizeof(patchdata_fields[0]), &response->response, &fields[2], "PatchData"))
    return RC_MISSING_VALUE;

  if (!rc_json_get_required_unum(&response->id, &response->response, &patchdata_fields[0], "ID"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->title, &response->response, &patchdata_fields[1], "Title"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->console_id, &response->response, &patchdata_fields[2], "ConsoleID"))
    return RC_MISSING_VALUE;

  /* ImageIcon will be '/Images/0123456.png' - only return the '0123456' */
  if (patchdata_fields[3].value_end) {
    str = patchdata_fields[3].value_end - 5;
    if (memcmp(str, ".png\"", 5) == 0) {
      patchdata_fields[3].value_end -= 5;

      while (str > patchdata_fields[3].value_start && str[-1] != '/')
        --str;

      patchdata_fields[3].value_start = str;
    }
  }
  rc_json_get_optional_string(&response->image_name, &response->response, &patchdata_fields[3], "ImageIcon", "");

  /* estimate the amount of space necessary to store the rich presence script, achievements, and leaderboards.
     determine how much space each takes as a string in the JSON, then subtract out the non-data (field names, punctuation)
     and add space for the structures. */
  len = patchdata_fields[4].value_end - patchdata_fields[4].value_start; /* rich presence */

  len += (patchdata_fields[5].value_end - patchdata_fields[5].value_start) - /* achievements */
          patchdata_fields[5].array_size * (130 - sizeof(rc_api_achievement_definition_t));

  len += (patchdata_fields[6].value_end - patchdata_fields[6].value_start) - /* leaderboards */
          patchdata_fields[6].array_size * (60 - sizeof(rc_api_leaderboard_definition_t));

  rc_buf_reserve(&response->response.buffer, len);
  /* end estimation */

  rc_json_get_optional_string(&response->rich_presence_script, &response->response, &patchdata_fields[4], "RichPresencePatch", "");
  if (!response->rich_presence_script)
    response->rich_presence_script = "";

  if (!rc_json_get_required_array(&response->num_achievements, &iterator, &response->response, &patchdata_fields[5], "Achievements"))
    return RC_MISSING_VALUE;

  if (response->num_achievements) {
    response->achievements = (rc_api_achievement_definition_t*)rc_buf_alloc(&response->response.buffer, response->num_achievements * sizeof(rc_api_achievement_definition_t));
    if (!response->achievements)
      return RC_OUT_OF_MEMORY;

    achievement = response->achievements;
    while (rc_json_get_array_entry_object(achievement_fields, sizeof(achievement_fields) / sizeof(achievement_fields[0]), &iterator)) {
      if (!rc_json_get_required_unum(&achievement->id, &response->response, &achievement_fields[0], "ID"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&achievement->title, &response->response, &achievement_fields[1], "Title"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&achievement->description, &response->response, &achievement_fields[2], "Description"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_unum(&achievement->category, &response->response, &achievement_fields[3], "Flags"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_unum(&achievement->points, &response->response, &achievement_fields[4], "Points"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&achievement->definition, &response->response, &achievement_fields[5], "MemAddr"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&achievement->badge_name, &response->response, &achievement_fields[7], "BadgeName"))
        return RC_MISSING_VALUE;

      len = achievement_fields[7].value_end - achievement_fields[7].value_start;
      if (len == last_author_len && memcmp(achievement_fields[7].value_start, last_author, len) == 0) {
        achievement->author = last_author;
      }
      else {
        if (!rc_json_get_required_string(&achievement->author, &response->response, &achievement_fields[6], "Author"))
          return RC_MISSING_VALUE;

        last_author = achievement->author;
        last_author_len = len;
      }

      if (!rc_json_get_required_unum(&timet, &response->response, &achievement_fields[8], "Created"))
        return RC_MISSING_VALUE;
      achievement->created = (time_t)timet;
      if (!rc_json_get_required_unum(&timet, &response->response, &achievement_fields[9], "Modified"))
        return RC_MISSING_VALUE;
      achievement->updated = (time_t)timet;

      ++achievement;
    }
  }

  if (!rc_json_get_required_array(&response->num_leaderboards, &iterator, &response->response, &patchdata_fields[6], "Leaderboards"))
    return RC_MISSING_VALUE;

  if (response->num_leaderboards) {
    response->leaderboards = (rc_api_leaderboard_definition_t*)rc_buf_alloc(&response->response.buffer, response->num_leaderboards * sizeof(rc_api_leaderboard_definition_t));
    if (!response->leaderboards)
      return RC_OUT_OF_MEMORY;

    leaderboard = response->leaderboards;
    while (rc_json_get_array_entry_object(leaderboard_fields, sizeof(leaderboard_fields) / sizeof(leaderboard_fields[0]), &iterator)) {
      if (!rc_json_get_required_unum(&leaderboard->id, &response->response, &leaderboard_fields[0], "ID"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&leaderboard->title, &response->response, &leaderboard_fields[1], "Title"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&leaderboard->description, &response->response, &leaderboard_fields[2], "Description"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&leaderboard->definition, &response->response, &leaderboard_fields[3], "Mem"))
        return RC_MISSING_VALUE;
      rc_json_get_optional_bool(&leaderboard->lower_is_better, &leaderboard_fields[5], "LowerIsBetter", 0);
      rc_json_get_optional_bool(&leaderboard->hidden, &leaderboard_fields[6], "Hidden", 0);

      if (!leaderboard_fields[4].value_end)
        return RC_MISSING_VALUE;
      len = leaderboard_fields[4].value_end - leaderboard_fields[4].value_start - 2;
      if (len < sizeof(format) - 1) {
        memcpy(format, leaderboard_fields[4].value_start + 1, len);
        format[len] = '\0';
        leaderboard->format = rc_parse_format(format);
      }
      else {
        leaderboard->format = RC_FORMAT_VALUE;
      }

      ++leaderboard;
    }
  }

  return RC_OK;
}

void rc_api_destroy_fetch_game_data_response(rc_api_fetch_game_data_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}

/* --- Ping --- */

int rc_api_init_ping_request(rc_api_request_t* request, const rc_api_ping_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request);

  if (api_params->game_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "ping", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);

    if (api_params->rich_presence && *api_params->rich_presence)
      rc_url_builder_append_str_param(&builder, "m", api_params->rich_presence);

    request->post_data = rc_url_builder_finalize(&builder);
  }

  return builder.result;
}

int rc_api_process_ping_response(rc_api_ping_response_t* response, const char* server_response) {
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"}
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  return rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
}

void rc_api_destroy_ping_response(rc_api_ping_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}

/* --- Award Achievement --- */

int rc_api_init_award_achievement_request(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params) {
  rc_api_url_builder_t builder;
  char buffer[33];
  md5_state_t md5;
  md5_byte_t digest[16];

  rc_api_url_build_dorequest_url(request);

  if (api_params->achievement_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 96);
  if (rc_api_url_build_dorequest(&builder, "awardachievement", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "a", api_params->achievement_id);
    rc_url_builder_append_unum_param(&builder, "h", api_params->hardcore ? 1 : 0);
    if (api_params->game_hash && *api_params->game_hash)
      rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);

    /* Evaluate the signature. */
    md5_init(&md5);
    snprintf(buffer, sizeof(buffer), "%u", api_params->achievement_id);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    md5_append(&md5, (md5_byte_t*)api_params->username, (int)strlen(api_params->username));
    snprintf(buffer, sizeof(buffer), "%d", api_params->hardcore ? 1 : 0);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    md5_finish(&md5, digest);
    rc_api_format_md5(buffer, digest);
    rc_url_builder_append_str_param(&builder, "v", buffer);

    request->post_data = rc_url_builder_finalize(&builder);
  }

  return builder.result;
}

int rc_api_process_award_achievement_response(rc_api_award_achievement_response_t* response, const char* server_response) {
  int result;
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"Score"},
    {"AchievementID"},
    {"AchievementsRemaining"}
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  if (!response->response.succeeded) {
    if (response->response.error_message &&
        memcmp(response->response.error_message, "User already has", 16) == 0) {
      /* not really an error, the achievement is unlocked, just not by the current call.
       *  hardcore:     User already has hardcore and regular achievements awarded.
       *  non-hardcore: User already has this achievement awarded.
       */
      response->response.succeeded = 1;
    } else {
      return result;
    }
  }

  rc_json_get_optional_unum(&response->new_player_score, &fields[2], "Score", 0);
  rc_json_get_optional_unum(&response->awarded_achievement_id, &fields[3], "AchievementID", 0);
  rc_json_get_optional_unum(&response->achievements_remaining, &fields[4], "AchievementsRemaining", (unsigned)-1);

  return RC_OK;
}

void rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}

/* --- Submit Leaderboard Entry --- */

int rc_api_init_submit_lboard_entry_request(rc_api_request_t* request, const rc_api_submit_lboard_entry_request_t* api_params) {
  rc_api_url_builder_t builder;
  char buffer[33];
  md5_state_t md5;
  md5_byte_t digest[16];

  rc_api_url_build_dorequest_url(request);

  if (api_params->leaderboard_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 96);
  if (rc_api_url_build_dorequest(&builder, "submitlbentry", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "i", api_params->leaderboard_id);
    rc_url_builder_append_num_param(&builder, "s", api_params->score);

    if (api_params->game_hash && *api_params->game_hash)
      rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);

    /* Evaluate the signature. */
    md5_init(&md5);
    snprintf(buffer, sizeof(buffer), "%u", api_params->leaderboard_id);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    md5_append(&md5, (md5_byte_t*)api_params->username, (int)strlen(api_params->username));
    snprintf(buffer, sizeof(buffer), "%d", api_params->score);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    md5_finish(&md5, digest);
    rc_api_format_md5(buffer, digest);
    rc_url_builder_append_str_param(&builder, "v", buffer);

    request->post_data = rc_url_builder_finalize(&builder);
  }

  return builder.result;
}

int rc_api_process_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response, const char* server_response) {
  rc_api_lboard_entry_t* entry;
  rc_json_field_t iterator;
  const char* str;
  int result;

  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"Response"} /* nested object */
  };

  rc_json_field_t response_fields[] = {
    {"Score"},
    {"BestScore"},
    {"RankInfo"}, /* nested object */
    {"TopEntries"} /* array */
    /* unused fields
    {"LBData"}, / * array * /
    {"ScoreFormatted"},
    {"TopEntriesFriends"}, / * array * /
      * unused fields */
  };

  /* unused fields
  rc_json_field_t lbdata_fields[] = {
    {"Format"},
    {"LeaderboardID"},
    {"GameID"},
    {"Title"},
    {"LowerIsBetter"}
  };
    * unused fields */

  rc_json_field_t entry_fields[] = {
    {"User"},
    {"Rank"},
    {"Score"}
    /* unused fields
    {"DateSubmitted"},
     * unused fields */
  };

  rc_json_field_t rank_info_fields[] = {
    {"Rank"},
    {"NumEntries"}
    /* unused fields
    {"LowerIsBetter"},
    {"UserRank"},
      * unused fields */
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_object(response_fields, sizeof(response_fields) / sizeof(response_fields[0]), &response->response, &fields[2], "Response"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_num(&response->submitted_score, &response->response, &response_fields[0], "Score"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_num(&response->best_score, &response->response, &response_fields[1], "BestScore"))
    return RC_MISSING_VALUE;

  if (!rc_json_get_required_object(rank_info_fields, sizeof(rank_info_fields) / sizeof(rank_info_fields[0]), &response->response, &response_fields[2], "RankInfo"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->new_rank, &response->response, &rank_info_fields[0], "Rank"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&str, &response->response, &rank_info_fields[1], "NumEntries"))
    return RC_MISSING_VALUE;
  response->num_entries = (unsigned)atoi(str);

  if (!rc_json_get_required_array(&response->num_top_entries, &iterator, &response->response, &response_fields[3], "TopEntries"))
    return RC_MISSING_VALUE;

  if (response->num_top_entries) {
    response->top_entries = (rc_api_lboard_entry_t*)rc_buf_alloc(&response->response.buffer, response->num_top_entries * sizeof(rc_api_lboard_entry_t));
    if (!response->top_entries)
      return RC_OUT_OF_MEMORY;

    entry = response->top_entries;
    while (rc_json_get_array_entry_object(entry_fields, sizeof(entry_fields) / sizeof(entry_fields[0]), &iterator)) {
      if (!rc_json_get_required_string(&entry->username, &response->response, &entry_fields[0], "User"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_unum(&entry->rank, &response->response, &entry_fields[1], "Rank"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_num(&entry->score, &response->response, &entry_fields[2], "Score"))
        return RC_MISSING_VALUE;

      ++entry;
    }
  }

  return RC_OK;
}

void rc_api_destroy_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}
