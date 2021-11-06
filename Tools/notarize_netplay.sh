#!/bin/zsh
#
# Signing and notarizing only happens on builds where the CI has access
# to the necessary secrets; this avoids builds in forks where secrets
# shouldn't be.

version="$(echo $GIT_TAG)"
identifier="com.project-slippi.dolphin"

requeststatus() { # $1: requestUUID
    requestUUID=${1?:"need a request UUID"}
    req_status=$(xcrun altool --notarization-info "$requestUUID" \
        --apiKey "${APPLE_API_KEY}" \
        --apiIssuer "${APPLE_ISSUER_ID}" 2>&1 \
    | awk -F ': ' '/Status:/ { print $2; }' )
    echo "$req_status"
}

logstatus() { # $1: requestUUID
    requestUUID=${1?:"need a request UUID"}
    xcrun altool --notarization-info "$requestUUID" \
        --apiKey "${APPLE_API_KEY}" \
        --apiIssuer "${APPLE_ISSUER_ID}"
    echo
}

notarizefile() { # $1: path to file to notarize, $2: identifier
    filepath=${1:?"need a filepath"}
    identifier=${2:?"need an identifier"}
    
    # upload file
    echo "## uploading $filepath for notarization"
    requestUUID=$(xcrun altool --notarize-app \
        --primary-bundle-id "$identifier" \
        --apiKey "${APPLE_API_KEY}" \
        --apiIssuer "${APPLE_ISSUER_ID}" \
        --file "$filepath" 2>&1 \
    | awk '/RequestUUID/ { print $NF; }')
                               
    echo "Notarization RequestUUID: $requestUUID"
    
    if [[ $requestUUID == "" ]]; then 
        echo "could not upload for notarization"
        exit 1
    fi
        
    # wait for status to be not "in progress" any more
    # Checks for up to ~10 minutes ((20 * 30s = 600) / 60s)
    for i ({0..20}); do
        request_status=$(requeststatus "$requestUUID")
        echo "Status: ${request_status}"

        # Why can this report two different cases...?
        if [ $? -ne 0 ] || [[ "${request_status}" =~ "invalid" ]] || [[ "${request_status}" =~ "Invalid" ]]; then
            logstatus "$requestUUID"
            echo "Error with notarization. Exiting!"
            exit 1
        fi

        if [[ "${request_status}" =~ "success" ]]; then
            logstatus "$requestUUID"
            echo "Successfully notarized! Stapling notarization status to ${filepath}"
            xcrun stapler staple "$filepath"
            exit 0
        fi

        echo "Still in progress, will check again in 30s"
        sleep 30
    done

    echo "Notarization request timed out - status below; maybe it needs more time?"
    logstatus "$requestUUID"
}

echo "Attempting notarization"
notarizefile "$1" "$identifier"
