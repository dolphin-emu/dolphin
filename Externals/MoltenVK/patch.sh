#!/bin/bash

# Applies all patches in the "patches" folder to the cloned MoltenVK git repository.
#
# Usage: patch.sh <patches folder> <MoltenVK version>
#

set -e

# Reset the git repository first to ensure that it's in the base state.
git reset --hard $2

git apply $1/*.patch
