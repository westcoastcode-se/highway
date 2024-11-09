FROM westcoastcode-se/highway/builder:latest AS builder

# Add Sources
ADD . /usr/src

# Build the source code
RUN set -ex; \
    cd /usr/src; \
    cmake . -DCMAKE_BUILD_TYPE=Release;  \
    cmake --build . --config Release --target examples_jc

# Copy example to it's own image
FROM alpine:latest

# Exposse the port and set volume
EXPOSE 8080
VOLUME /data
COPY --from=builder /usr/src/examples_jc /examples_jc
ENTRYPOINT ["/examples_jc"]
