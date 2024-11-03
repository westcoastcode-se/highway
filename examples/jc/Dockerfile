FROM gcc:14.2.0 AS builder

# Add Sources
ADD . /usr/src

# Download neccessary files and build the soruce code
RUN set -ex; \
    apt-get update; \
    apt-get install -y cmake libzmq3-dev unzip; \
    cd /usr/src; \
    cmake .; cmake --build . --target examples_jc

# Copy example to it's own image \
FROM alpine:latest

# Exposse the port and set volume
EXPOSE 8080
VOLUME /data
COPY --from=builder /usr/src/examples_jc /examples_jc
ENTRYPOINT ["/examples_jc"]
