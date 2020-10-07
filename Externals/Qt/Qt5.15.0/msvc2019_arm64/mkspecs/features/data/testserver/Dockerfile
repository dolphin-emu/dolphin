# This Dockerfile is used to provision the shared scripts (e.g. startup.sh) and configurations. It
# relies on the arguments passed by docker-compose file to build additional images for each service.
# To lean more how it works, please check the topic "Use multi-stage builds".
# https://docs.docker.com/develop/develop-images/multistage-build/

ARG provisioningImage
FROM $provisioningImage as testserver_tier2

# Add and merge the testdata into service folder
ARG serviceDir
ARG shareDir=$serviceDir
COPY $serviceDir $shareDir service/

# create the shared script of testserver
RUN echo "#!/usr/bin/env bash\n" \
         "set -ex\n" \
         "for RUN_CMD; do \$RUN_CMD; done\n" \
         "service dbus restart\n" \
         "service avahi-daemon restart\n" \
         "sleep infinity\n" > startup.sh
RUN chmod +x startup.sh

# rewrite the default configurations of avahi-daemon
# Disable IPv6 of avahi-daemon to resolve the unstable connections on Windows
ARG test_domain
RUN sed -i -e "s,#domain-name=local,domain-name=${test_domain:-test-net.qt.local}," \
        -e "s,#publish-aaaa-on-ipv4=yes,publish-aaaa-on-ipv4=no," \
        -e "s,use-ipv6=yes,use-ipv6=no," \
        /etc/avahi/avahi-daemon.conf
