#! /bin/bash
#
# Linter script that checks for common style issues in Dolphin's codebase.

set -euo pipefail

fail=0

# Default to staged files, unless a commit was passed.
COMMIT=${1:---cached}

# Get modified files (must be on own line for exit-code handling)
modified_files=$(git diff --name-only --diff-filter=ACMRTUXB $COMMIT)

# Loop through each modified file.
for f in ${modified_files}; do
  # Filter them.
  if ! echo "${f}" | egrep -q "[.](cpp|h|mm)$"; then
    continue
  fi
  if ! echo "${f}" | egrep -q "^Source"; then
    continue
  fi

  # Check for clang-format issues.
  d=$(clang-format ${f} | (diff -u "${f}" - || true))
  if ! [ -z "${d}" ]; then
    echo "!!! ${f} not compliant to coding style, here is the fix:"
    echo "${d}"
    fail=1
  fi

  # Check for newline at EOF.
  last_line="$(tail -c 1 ${f})"
  if [ -n "${last_line}" ]; then
    echo "!!! ${f} not compliant to coding style:"
    echo "Missing newline at end of file"
    fail=1
  fi
done

exit ${fail}
