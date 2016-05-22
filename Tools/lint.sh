#! /bin/sh
#
# Linter script that checks for common style issues in Dolphin's codebase.

REPO_BASE=$(realpath $(dirname $0)/..)

fail=0

# Step 1: check for includes ordering.
echo "[.] Checking #include statements ordering."
res=$(
  ${REPO_BASE}/Tools/check-includes.py \
      $(find ${REPO_BASE}/Source/Core -name "*.cpp" -o -name "*.h") \
      2>&1 \
      >/dev/null
)
if [ -n "${res}" ]; then
  echo "FAIL: ${res}"
  fail=1
else
  echo "OK"
fi

# Step 2: check for trailing whitespaces.
echo "[.] Checking for trailing whitespaces."
res=$(
    find ${REPO_BASE}/Source/Core -name "*.cpp" -o -name "*.h" \
        | xargs egrep -n "\s+$"
)
if [ -n "$res" ]; then
  echo "FAIL: ${res}"
  fail=1
else
  echo "OK"
fi

exit ${fail}
