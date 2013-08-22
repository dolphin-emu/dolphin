#! /bin/bash

if [ "$#" -ne 1 ]; then
    echo >&2 "usage: $0 <gameini-file>"
    exit 1
fi

[ -z "$PGPASSWORD" ] && read -rs -p 'Enter PostgreSQL password: ' PGPASSWORD

export PGHOST=postgresql1.alwaysdata.com
export PGDATABASE=dolphin-emu_wiki
export PGUSER=dolphin-emu_wiki
export PGPASSWORD

sql() {
    psql -A -t -F ',' -c "$1"
}

GAME_ID=$(basename "$1" | cut -c -6)

if ! echo "$GAME_ID" | grep -q '[A-Z0-9]\{6\}'; then
    echo >&2 "Invalid game ID: $GAME_ID"
    exit 1
fi

GAME_ID_GLOB=$(echo "$GAME_ID" | sed 's/\(...\).\(..\)/\1_\2/')
RATING=$(sql "
    SELECT
        rating_content.old_text
    FROM
        page gid_page
    LEFT JOIN pagelinks gid_to_main
        ON gid_to_main.pl_from = gid_page.page_id
    LEFT JOIN page rating_page
        ON rating_page.page_title = ('Ratings/' || gid_to_main.pl_title)
    LEFT JOIN revision rating_rev
        ON rating_rev.rev_id = rating_page.page_latest
    LEFT JOIN pagecontent rating_content
        ON rating_content.old_id = rating_rev.rev_text_id
    WHERE
        gid_page.page_title LIKE '$GAME_ID_GLOB'
    LIMIT 1
" | grep '^[1-5]$')

if ! [ -z "$RATING" ]; then
    sed -i "s/^EmulationStateId.*$/EmulationStateId = $RATING/" "$1"
fi
