FROM westcoastcode-se/highway/builder:latest AS builder

# Add Sources
ADD . /usr/src

# Build the source code
RUN set -ex; \
    cd /usr/src; \
    cmake .;  \
    cmake --build . --config Release --target examples_boot

# Copy example to it's own image
FROM scratch

# Exposse the port and set volume
EXPOSE 8080

# Copy files
COPY --from=builder /usr/src/examples_boot /examples_boot

# Start with /data folder
ENTRYPOINT ["/examples_boot"]
