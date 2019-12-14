#!/bin/sh

set -e

chown root:wheel /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist
launchctl load -w /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist
