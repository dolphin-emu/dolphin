#!/usr/bin/env sh

KEY_CHAIN=build.keychain
CERTIFICATE_P12=certificate.p12

echo "(cert-loader) About to b64 decode cert"

# Recreate the certificate from the secure environment variable
echo $CERTIFICATE_MACOS_APPLICATION | base64 --decode > $CERTIFICATE_P12

echo "(cert-loader) About to create keychain"

# Create a temporary keychain
security create-keychain -p actions $KEY_CHAIN

echo "(cert-loader) About to set keychain settings"

# Remove the relock timeout, which can happen if our builds take forever.
security set-keychain-settings $KEY_CHAIN

echo "(cert-loader) About to import cert"

# Import certificate
security import $CERTIFICATE_P12 -k $KEY_CHAIN -P $CERTIFICATE_MACOS_PASSWORD -T /usr/bin/codesign;

echo "(cert-loader) About to allow access from tools"

# Mark this as okay to be accessed from command line tools
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k actions $KEY_CHAIN

echo "(cert-loader) About to make default keychain"

# Make the keychain the default so identities are found
security default-keychain -s $KEY_CHAIN

echo "(cert-loader) About to unlock keychain"

# Unlock the keychain
security unlock-keychain -p actions $KEY_CHAIN

echo "(cert-loader) Remove intermediary certs"

# remove certs
rm -fr *.p12
