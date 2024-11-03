FROM gcc:14.2.0 AS builder

# Add Sources
ADD . /usr/src

# Download neccessary files and build the soruce code
RUN set -ex; \
    apt-get update; \
    apt-get install -y cmake libzmq3-dev unzip; \
    cd /usr/src; \
    cmake .; cmake --build . --target examples_hello_world

# Copy example to it's own image \
FROM alpine:latest

# Install GCC libraries
# TODO Add support for statically linked so that we don't have to do this
RUN apk add gcompat

COPY --from=builder /usr/src/examples_hello_world /examples_hello_world
ENTRYPOINT ["/examples_hello_world"]
