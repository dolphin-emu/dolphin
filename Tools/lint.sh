#! /bin/bash
#
# Linter script that checks for common style issues in Dolphin's codebase.

fail=0

# Loop through each modified file.
for f in $(git diff --name-only --diff-filter=ACMRTUXB --cached); do
  # Filter them.
  if ! echo "${f}" | egrep -q "[.](cpp|h|mm)$"; then
    continue
  fi
  if ! echo "${f}" | egrep -q "^Source/Core"; then
    continue
  fi

  # Check for clang-format issues.
  d=$(diff -u "${f}" <(clang-format ${f}))
  if ! [ -z "${d}" ]; then
    echo "!!! ${f} not compliant to coding style, here is the fix:"
    echo "${d}"
    fail=1
  fi

  # Check for newline at EOF.
  if [ -n "$(tail -c 1 ${f})" ]; then
    echo "!!! ${f} not compliant to coding style:"
    echo "Missing newline at end of file"
    fail=1
  fi
done

exit ${fail}
