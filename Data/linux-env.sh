#!/bin/bash -e
# linux-env.sh

# Add /usr/lib/ to LD_LIBRARY_PATH cause Ubuntu is dumb
export LD_LIBRARY_PATH="/usr/lib/:$LD_LIBRARY_PATH"

if [[ $(env | grep -i wayland) ]]; then
    # wxWidgets 3.14 is GTK3, which seemingly has an issue or two when
    # running under Wayland. Explicitly setting this for Slippi avoids
    # those issues.
    export GDK_BACKEND=x11
    
    # Disable Webkit compositing on Wayland cause it breaks stuff
    export WEBKIT_DISABLE_COMPOSITING_MODE=1
fi
