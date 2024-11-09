FROM westcoastcode-se/highway/builder:latest AS builder

# Add Sources
ADD . /usr/src

# Build the source code
RUN set -ex; \
    cd /usr/src; \
    cmake . -DCMAKE_BUILD_TYPE=Release;  \
    cmake --build . --config Release --target examples_static

# Copy example to it's own image
FROM scratch

# Exposse the port and set volume
EXPOSE 8080
VOLUME /data

# Copy files
COPY --from=builder /usr/src/examples_static /examples_static

# Start with /data folder
ENTRYPOINT ["/examples_static", "/data"]
