#!/bin/sh

set -e

echo "Unloading csdbgd (preinst)"
launchctl unload /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist
