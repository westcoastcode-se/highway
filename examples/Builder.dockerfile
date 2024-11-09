#
# Base image for building Highway Applications. Will contain the neccessary tools for compiling C and
# C++ applications using GCC
#
FROM gcc:14.2.0

# Download necessary files and build the source code
RUN set -ex; \
    apt-get update; \
    apt-get install -y cmake libzmq3-dev unzip;
