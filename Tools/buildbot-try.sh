#! /bin/bash
#
# Submits a "buildbot try" message to the Dolphin buildbot with all the
# required options.

opt_file=$HOME/.buildbot/options

if ! [ -f "$opt_file" ]; then
    echo >&2 "error: no .buildbot/options configuration file found"
    echo >&2 "Read the docs: https://wiki.dolphin-emu.org/index.php?title=Buildbot"
    exit 1
fi

if ! which buildbot >/dev/null 2>&1; then
    echo >&2 "error: buildbot is not installed"
    echo >&2 "Install it from your package manager, or use 'pip install buildbot'"
    exit 1
fi

if ! git branch | grep -q '^* '; then
    echo "Unable to determine the current Git branch. Input the Git branch name:"
    read branchname
else
    branchname=$(git branch | grep '^* ' | cut -d ' ' -f 2-)
fi

shortrev=$(git describe --always --long --dirty=+ | sed 's/-g[0-9a-f]*\(+*\)$/\1/')

author=$(grep try_username "$opt_file" | cut -d "'" -f 2)

remote=$(git remote -v | grep dolphin-emu/dolphin.git | head -n1 | cut -f1)
remote=${remote:-origin}

baserev=$(git merge-base HEAD $remote/master)

echo "Branch name: $branchname"
echo "Change author: $author"
echo "Short rev: $shortrev"
echo "Remote: $remote"
echo "Base rev: $baserev"

git diff --binary -r $baserev | buildbot try --properties=branchname=$branchname,author=$author,shortrev=$shortrev --diff=- -p1 --baserev $baserev $*
