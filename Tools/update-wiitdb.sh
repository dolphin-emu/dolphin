#!/bin/bash
# Script to update the bundled WiiTDBs and remove duplicate entries.

set -eu
ENGLISH_DB_NAME="Data/Sys/wiitdb-en.txt"
ENGLISH_TMP_DB_NAME="Data/Sys/wiitdb-en.tmp"
TIMESTAMP_PATTERN="TITLES = "

echo "Downloading WiiTDB (en)..."
curl -s "http://www.gametdb.com/wiitdb.txt?LANG=EN" | sort > "$ENGLISH_TMP_DB_NAME"

dbs=()
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=DE de")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=ES es")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=FR fr")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=IT it")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=JA ja")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=KO ko")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=NL nl")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=PT pt")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=RU ru")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=ZHCN zh_CN")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=ZHTW zh_TW")

for i in "${dbs[@]}"
do
  entry=(${i// / })
  url="${entry[0]}"
  lang_code="${entry[1]}"

  dest="Data/Sys/wiitdb-$lang_code.txt"
  tmp_dest="Data/Sys/wiitdb-$lang_code.tmp"
  echo "Downloading WiiTDB ($lang_code)..."
  curl -s "$url" | sort | comm -23 - "$ENGLISH_TMP_DB_NAME" > "$tmp_dest"
  grep "$TIMESTAMP_PATTERN" "$tmp_dest" > "$dest"
  grep -v "$TIMESTAMP_PATTERN" "$tmp_dest" >> "$dest"
  rm "$tmp_dest"
done

grep "$TIMESTAMP_PATTERN" "$ENGLISH_TMP_DB_NAME" > "$ENGLISH_DB_NAME"
grep -v "$TIMESTAMP_PATTERN" "$ENGLISH_TMP_DB_NAME" >> "$ENGLISH_DB_NAME"
rm "$ENGLISH_TMP_DB_NAME"
