#! /bin/bash
#
# Script that automatically fixes common style issues which can be used as pre-commit hook.

for f in $(git diff --cached --name-only); do
  if ! echo "${f}" | egrep -q "[.](cpp|h|mm)$"; then
    continue
  fi
  if ! echo "${f}" | egrep -q "^Source/Core"; then
    continue
  fi
  clang-format -style=file -i "${f}"
  git add "${f}"
  diff=$(git diff --cached)
  if [ -z "${diff}" ]; then
    echo "No change to ${f} after reformatting."
    exit 1
  fi
done
