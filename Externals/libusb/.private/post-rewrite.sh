#!/bin/sh
#
# Detect amended commits and warn user if .amend is missing
#
# To have git run this script on commit, create a "post-rewrite" text file in
# .git/hooks/ with the following content:
# #!/bin/sh
# if [ -x .private/post-rewrite.sh ]; then
#   source .private/post-rewrite.sh
# fi
#
# NOTE: These versioning hooks are intended to be used *INTERNALLY* by the
# libusb development team and are NOT intended to solve versioning for any
# derivative branch, such as one you would create for private development.
#

case "$1" in
  amend)
    # Check if a .amend exists. If none, create one and warn user to re-commit.
    if [ -f .amend ]; then
      rm .amend
    else
      echo "Amend commit detected, but no .amend file - One has now been created."
      echo "Please re-commit as is (amend), so that the version number is correct."
      touch .amend
    fi ;;
  *) ;;
esac
