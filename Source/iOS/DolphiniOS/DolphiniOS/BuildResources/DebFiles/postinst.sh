#!/bin/sh

set -e

echo "Setting csdbgd permissions"
chown root:wheel /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist

echo "Loading csdbgd"
launchctl load -w /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist

echo "Creating user directory if needed"
mkdir /private/var/mobile/Documents/DolphiniOS || true

echo "Setting user directory permissions"
chown -R mobile:mobile /private/var/mobile/Documents/DolphiniOS

uicache
