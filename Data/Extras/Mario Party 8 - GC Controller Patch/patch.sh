#!/bin/bash

# Get a list of ISO and WBFS files in the same directory as the script
iso_files=$(find "$(dirname "$0")" -maxdepth 1 -type f \( -iname "*.iso" -o -iname "*.wbfs" \))

# Check if there are no ISO or WBFS files
if [ -z "$iso_files" ]; then
  echo ""
  echo "No ISO or WBFS files found in the same directory as the script."
  echo "Place the Mario Party 8 (US) ISO or WBFS in the script's directory and run it again."
  echo ""
  read -p "Press Enter to exit..."
  exit 0
fi

# Iterate over each ISO or WBFS file and process it
for originalISO in $iso_files; do
  echo ""
  echo "Constructing the Mario Party 8 GC Controller WBFS for \"$originalISO\". Please stand by...."

  cd "$(dirname "$0")/tools"

  # Ensure temp directory exists
  mkdir -p temp

  wit extract "$originalISO" --dest=temp

  if [ -d "temp/DATA" ]; then
    cp -r "../mp8motion" "temp/DATA"
  else
    cp -r "../mp8motion" "temp"
  fi

  wit copy "temp" "../Mario Party 8 (USA) [GameCube Controller v7].wbfs"
  rm -rf temp

  echo ""
  echo "Construction complete for \"$originalISO\"!"
done

echo ""
read -p "Press Enter to exit..."
exit 0
