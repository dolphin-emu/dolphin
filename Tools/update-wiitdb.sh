#!/bin/bash
# Script to update the bundled WiiTDBs.

set -eu

dbs=()
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=EN en")
# UNIQUE means "only return non-English titles".
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=DE&UNIQUE=TRUE de")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=ES&UNIQUE=TRUE es")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=FR&UNIQUE=TRUE fr")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=IT&UNIQUE=TRUE it")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=JA&UNIQUE=TRUE ja")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=KO&UNIQUE=TRUE ko")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=NL&UNIQUE=TRUE nl")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=PT&UNIQUE=TRUE pt")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=RU&UNIQUE=TRUE ru")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=ZHCN&UNIQUE=TRUE zh_CN")
dbs+=("http://www.gametdb.com/wiitdb.txt?LANG=ZHTW&UNIQUE=TRUE zh_TW")

for i in "${dbs[@]}"
do
  entry=(${i// / })
  url="${entry[0]}"
  lang_code="${entry[1]}"

  dest="Data/Sys/wiitdb-$lang_code.txt"
  echo "Downloading WiiTDB ($lang_code)..."
  curl -s "$url" > "$dest"
done
