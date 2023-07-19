# syntax=docker/dockerfile:1
FROM sbchoi/glet-base:latest
# install ping for testing
RUN apt-get install -y iputils-ping

WORKDIR /root/org/gpu-let
# copy all source code / scripts
COPY tmp/src ./src
#COPY tmp/scripts ./scripts
COPY tmp/CMakeLists.txt ./

# build code
RUN mkdir -p /root/org/gpu-let/sripts
COPY tmp/scripts/build_all.sh scripts/build_all.sh
WORKDIR /root/org/gpu-let/scripts
RUN ./build_all.sh

# enable passwd access
RUN sed -i 's/PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

CMD service ssh start && /bin/bash
