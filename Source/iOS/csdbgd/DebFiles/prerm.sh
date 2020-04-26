#!/bin/sh

set -e

echo "Unloading csdbgd (prerm)"
launchctl unload -w /Library/LaunchDaemons/me.oatmealdome.csdbgd.plist || true
