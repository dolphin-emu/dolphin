#!/bin/sh

set -e

echo "Setting csdbgd permissions"
chown root:wheel /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist

echo "Loading csdbgd"
launchctl load -w /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist