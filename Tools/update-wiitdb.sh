#!/bin/bash
# Script to update the bundled WiiTDBs.

set -eu

dbs=()
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=EN en")
# UNIQUE means "only return non-English titles".
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=DE&UNIQUE=TRUE de")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=ES&UNIQUE=TRUE es")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=FR&UNIQUE=TRUE fr")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=IT&UNIQUE=TRUE it")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=JA&UNIQUE=TRUE ja")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=KO&UNIQUE=TRUE ko")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=NL&UNIQUE=TRUE nl")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=PT&UNIQUE=TRUE pt")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=RU&UNIQUE=TRUE ru")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=ZHCN&UNIQUE=TRUE zh_CN")
dbs+=("https://www.gametdb.com/wiitdb.txt?LANG=ZHTW&UNIQUE=TRUE zh_TW")

for i in "${dbs[@]}"
do
  entry=(${i// / })
  url="${entry[0]}"
  lang_code="${entry[1]}"

  dest="Data/Sys/wiitdb-$lang_code.txt"
  echo "Downloading WiiTDB ($lang_code)..."
  curl -s "$url" > "$dest"
done
