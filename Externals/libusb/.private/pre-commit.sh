#!/bin/sh
#
# Sets the nano version according to the number of commits on this branch, as
# well as the branch offset.
#
# To have git run this script on commit, first make sure you change
# BRANCH_OFFSET to 60000 or higher, then create a "pre-commit" text file in
# .git/hooks/ with the following content:
# #!/bin/sh
# if [ -x .private/pre-commit.sh ]; then
#   source .private/pre-commit.sh
# fi
#
# NOTE: These versioning hooks are intended to be used *INTERNALLY* by the
# libusb development team and are NOT intended to solve versioning for any
# derivative branch, such as one you would create for private development.
#
# Should you wish to reuse these scripts for your own versioning, in your own
# private branch, we kindly ask you to first set BRANCH_OFFSET to 60000, or
# higher, as any offset below below 60000 is *RESERVED* for libusb official
# usage.

################################################################################
##  YOU *MUST* SET THE FOLLOWING TO 60000 OR HIGHER IF YOU REUSE THIS SCRIPT  ##
################################################################################
BRANCH_OFFSET=10000
################################################################################

if [ "$BASH_VERSION" = '' ]; then
  TYPE_CMD="type git >/dev/null 2>&1"
else
  TYPE_CMD="type -P git &>/dev/null"
fi

eval $TYPE_CMD || { echo "git command not found. Aborting." >&2; exit 1; }

NANO=`git log --oneline | wc -l`
NANO=`expr $NANO + $BRANCH_OFFSET`
# Amended commits need to have the nano corrected. Current versions of git hooks
# only allow detection of amending post commit, so we require a .amend file,
# which will be created post commit with a user warning if none exists when an
# amend is detected.
if [ -f .amend ]; then
  NANO=`expr $NANO - 1`
fi
echo "setting nano to $NANO"
echo "#define LIBUSB_NANO $NANO" > libusb/version_nano.h
git add libusb/version_nano.h
