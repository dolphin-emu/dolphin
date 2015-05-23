#!/bin/bash
# Updates a file's license header to the first year a commit touched it.
# Example: find -name \*.cpp -o -name \*.h|xargs -n1 -P8 /path/to/Tools/update-license-headers.sh

filename=$1
echo "Updating $filename..."
year=$(git log --diff-filter=A --follow --format=%ad --date=short "$1"|tail -n1|cut -d- -f1)
if [[ "$(head -n1 "$filename" | grep Copyright)" == "" ]]
then
    tmp="$(mktemp)"
    echo -e "// Copyright $year Dolphin Emulator Project\n// Licensed under GPLv2+\n// Refer to the license.txt file included.\n"|cat - "$filename" >"$tmp"
    mv "$tmp" "$filename"
else
    sed -ri "1 s|// Copyright ([0-9-]+) Dolphin Emulator Project|// Copyright $year Dolphin Emulator Project|" "$filename"
fi
