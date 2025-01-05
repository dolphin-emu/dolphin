#!/bin/zsh
#
# Signing and notarizing only happens on builds where the CI has access
# to the necessary secrets; this avoids builds in forks where secrets
# shouldn't be.
#
# Portions of the notarization response checks are borrowed from:
#
# https://github.com/smittytone/scripts/blob/main/packcli.zsh
#
# (They've done the work of figuring out what the reponse formats are, etc)

version="$(echo $GIT_TAG)"
identifier="com.project-slippi.dolphin-beta"
filepath=${1:?"need a filepath"}

echo "Attempting notarization"

# Submit the DMG for notarization and wait for the flow to finish
s_time=$(date +%s)
response=$(xcrun notarytool submit ${filepath} \
    --wait \
    --issuer ${APPLE_ISSUER_ID} \
    --key-id ${APPLE_API_KEY} \
    --key ~/private_keys/AuthKey_${APPLE_API_KEY}.p8)

# Get the notarization job ID from the response
job_id_line=$(grep -m 1 '  id:' < <(echo -e "${response}"))
job_id=$(echo "${job_id_line}" | cut -d ":" -s -f 2 | cut -d " " -f 2)

# Log some debug timing info.
e_time=$(date +%s)
n_time=$((e_time - s_time))
echo "Notarization call completed after ${n_time} seconds. Job ID: ${job_id}"

# Extract the status of the notarization job.
status_line=$(grep -m 1 '  status:' < <(echo -e "${response}"))
status_result=$(echo "${status_line}" | cut -d ":" -s -f 2 | cut -d " " -f 2)

# Fetch and echo the log *before* bailing if it's bad, so we can tell if there's
# a deeper error we need to handle.
log_response=$(xcrun notarytool log \
    --issuer ${APPLE_ISSUER_ID} \
    --key-id ${APPLE_API_KEY} \
    --key ~/private_keys/AuthKey_${APPLE_API_KEY}.p8 \
    ${job_id})
echo "${log_response}"

if [[ ${status_result} != "Accepted" ]]; then
    echo "Notarization failed with status ${status_result}"
    exit 1
fi

# Attempt to staple the notarization result to the app.
echo "Successfully notarized! Stapling notarization status to ${filepath}"
success=$(xcrun stapler staple "${filepath}")
if [[ -z "${success}" ]]; then
    echo "Could not staple notarization to app"
    exit 1
fi

# Confirm the staple actually worked...
echo "Checking notarization to ${filepath}"
spctl --assess -vvv --type install "${filepath}"
