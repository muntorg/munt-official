# Build Munt ()
#
# VERSION               0.1

FROM ubuntu:18.04

# Generic build tools
RUN apt-get update && apt-get install  -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    curl \
    git \
    ca-certificates \
    ccache \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/* 

# SDK prerequisites
RUN apt-get update && apt-get install  -y \
    libqrencode-dev \
    libminiupnpc-dev \
    python3-zmq \
    protobuf-compiler \
    libdbus-1-dev \
    libharfbuzz-dev \
    libprotobuf-dev \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/* 

ADD *.sh /root/
CMD /root/build.sh
