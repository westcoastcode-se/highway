FROM westcoastcode-se/highway/builder:latest AS builder

# Add Sources
ADD . /usr/src

# Build the source code
RUN set -ex; \
    cd /usr/src; \
    cmake .;  \
    cmake --build . --config Release --target examples_hello_world

# Copy example to it's own image
FROM alpine:latest

# Install GCC libraries
# TODO Add support for statically linked so that we don't have to do this
RUN apk add gcompat

COPY --from=builder /usr/src/examples_hello_world /examples_hello_world
ENTRYPOINT ["/examples_hello_world"]
