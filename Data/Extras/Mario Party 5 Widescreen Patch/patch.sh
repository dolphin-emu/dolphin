#!/bin/bash

# Get a list of ISO files in the same directory as the script
iso_files=$(find "$(dirname "$0")" -maxdepth 1 -type f -iname "*.iso")

# Check if there are no ISO files
if [ -z "$iso_files" ]; then
  echo ""
  echo "No ISO files found in the same directory as the script."
  echo "Place the Mario Party 5 (USA) ISO in the script's directory and run it again."
  echo ""
  read -p "Press Enter to exit..."
  exit 0
fi

# Iterate over each ISO file and process it
for originalISO in $iso_files; do
  echo ""
  echo "Given \"$originalISO\""
  xdelta3 -d -s "$originalISO" data.xdelta3 "Mario Party 5 (USA) [Widescreen].iso"
  echo "Press Enter to exit."
  read -p ""
done

echo ""
read -p "Press Enter to exit..."
exit 0