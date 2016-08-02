#! /bin/bash
#
# Linter script that checks for common style issues in Dolphin's codebase.

fail=0

# Check for clang-format issues.
for f in $(git diff --name-only --diff-filter=ACMRTUXB); do
  if ! echo "${f}" | egrep -q "[.](cpp|h|mm)$"; then
    continue
  fi
  if ! echo "${f}" | egrep -q "^Source/Core"; then
    continue
  fi
  d=$(diff -u "${f}" <(clang-format ${f}))
  if ! [ -z "${d}" ]; then
    echo "!!! ${f} not compliant to coding style, here is the fix:"
    echo "${d}"
    fail=1
  fi
done

exit ${fail}
