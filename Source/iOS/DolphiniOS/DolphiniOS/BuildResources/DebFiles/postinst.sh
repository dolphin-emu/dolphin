#!/bin/sh

set -e

echo "Setting csdbgd permissions"
chown root:wheel /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist

echo "Loading csdbgd"
launchctl load -w /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist

# Seriously, WTF is going on here? I'm really curious what app/tweak
# does this and why.
echo "Checking if Documents is a file"
if [ -f "/private/var/mobile/Documents" ]; then
  rm /private/var/mobile/Documents
fi

echo "Creating user directory if needed"
mkdir -p /private/var/mobile/Documents/DolphiniOS || true

echo "Setting user directory permissions"
chown -R mobile:mobile /private/var/mobile/Documents/DolphiniOS

uicache
