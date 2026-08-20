# This file exists to be consumed via `add_custom_command`
# so that file changes are picked up without needing a full reconfigure

file(SHA1 ${JSON_FILE} ACHIEVEMENT_APPROVED_LIST_HASH)
configure_file(${TEMPLATE_FILE} ${OUTPUT_FILE} @ONLY)
