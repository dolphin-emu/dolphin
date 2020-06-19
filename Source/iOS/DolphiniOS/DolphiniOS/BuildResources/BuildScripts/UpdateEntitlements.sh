#!/bin/bash

exit 0

set -e

XCENT_FILE=$DERIVED_SOURCES_DIR/../DolphiniOS.app.xcent

# dynamic-codesigning is needed
if [ $BUILD_FOR_JAILBROKEN_DEVICE == "YES" ]
then
  /usr/libexec/PlistBuddy -x -c "Merge $PROJECT_DIR/DolphiniOS/BuildResources/Entitlements_JB.plist" "$XCENT_FILE"
else
  grep -q "get-task-allow" "$XCENT_FILE"
  if [[ $? != 0 ]];
  then
    echo "        <key>get-task-allow</key>" >> "$XCENT_FILE"
    echo "        <true/>" >> "$XCENT_FILE"
  fi
  
  sed -i '' -e '$d' "$XCENT_FILE"
  sed -i '' -e '$d' "$XCENT_FILE"
  echo "`cat $PROJECT_DIR/DolphiniOS/BuildResources/Entitlements_PsychicPaper.txt`" >> "$XCENT_FILE"
fi
