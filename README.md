The goal is for highway is to become a low-memory, high-performance HTTP framework for C applications
that can be used to build REST applications.

# Features

**Implemented**

- [x] HTTP/1.1 Standard: Understand what http method client is using (GET, POST, PUT, DELETE, OPTION)
- [x] Header: Content-Length
- [x] Header: Content-Type
- [x] IP Version: IPv4
- [x] IP Version: IPv6 and IPv6 at the same time
- [x] Logging: The client IP
- [x] Support for filter chains
- [x] Support for servlet function
- [x] Library: Easier way to use the framework using Highway Boot
- [x] Build: Preliminary Docker support
- [x] Library: Serving files from the disk used for static html content

**Not implemented**

- [ ] Security: HTTPS
- [ ] Security: Blocking IP-addresses and ranges
- [ ] Cache Control
- [ ] HttpStatus: 304 Not Modified in case of `If-Modified-Since` header
- [ ] Security: CORS (Cross-Origin Resource Sharing) support for increased protection
- [ ] Security: CSRF (Cross-Site Request Forgery)
- [ ] Security: SSRF (Server-Side Request Forgery)
- [ ] Test: Add a way to mock requests for unit-tests
- [ ] Library: Shared Library and DllExport for Windows, Linux and OSX platforms
- [ ] Security: Protection against 1-byte TCP messages
- [ ] Security: Auto-blocking IP-addresses for a short time
- [ ] Logging: Add support for thread-safe logging
- [ ] Performance: Detaching the servlet function if the response content is very large
  - Example: `hiw_servlet_detach(req, resp, new_func)` should return and continue running in the new function letting
    the acceptor thread continue accepting new connections
  - Allow for configuring the maximum number of deatched threads
- [ ] Compatibility: Add support for most common character-encoding identifications for text-based mime-types
      such as UTF-8 and ISO 8859-1 if part of `Content-Type` header (text/html; charset=utf-8)
- [ ] Security: Memory Leak Detection
- [ ] Build: CMake options for all default definitions, such as maximum header size
- [ ] Library: Add a Highway client implementation: `hiw_http_client`
  - Could be useful adding `detach` support for this, in case of slow IO
- [ ] Library: Expose multiple server ports, for example a management port for health
- [ ] Build: Allow for embedding static content directly in the binary
- [ ] Library: Reverse proxy support
- [ ] ...

# Examples

You can start by clone the entire source code:

```bash
git clone https://github.com/westcoastcode-se/highway.git
```

Then build the `builder` image:

```bash
docker build -f examples/Builder.dockerfile -t westcoastcode-se/highway/builder:latest .
```

Then follow the examples below

## (hello_world) Hello World

A simple hello world example. Exposes http://127.0.0.1:8080

### Build

```bash
docker build -f examples/hello_world/Dockerfile -t westcoastcode-se/highway/examples_hello_world:latest .
docker run --rm -it -p 8080:8080 westcoastcode-se/highway/examples_hello_world:latest
```

## (static) Static

A tiny static content server. Exposes on http://127.0.0.1:8080

### Build

```bash
docker build -f examples/static/Dockerfile -t westcoastcode-se/highway/examples_static:latest .
docker run --rm -it -p 8080:8080 -v $(pwd)/examples/static/data:/data westcoastcode-se/highway/examples_static:latest
```

### Usage

```bash
Usage: static [data-dir] [max-threads] [read-timeout] [write-timeout]

  data-dir
```

## ([boot](examples/boot/main.c)) Hello World using Highway Boot

A tiny rest server using Highway Boot. Exposes on http://127.0.0.1:8080

### Build

```bash
docker build -f examples/boot/Dockerfile -t westcoastcode-se/highway/examples_boot:latest .
docker run --rm -it -p 8080:8080 westcoastcode-se/highway/examples_boot:latest
```

## ([jc](examples/jc/main.c)) Json Cache

A json cache server using Highway Boot. Exposes on http://127.0.0.1:8080

Please note that THIS SERVER IS NOT SUPPOSED TO RUN IN ANY PRODUCTION ENVIRONMENT. The purpose for this application is to make it easy to do basic JSON methods in a small and easy-to-start application.

All content is stored in the data folder. This folder is placed in the same directory as where the executable is running from. The server itself is exposed over port 8080 and will accept incoming connections from any IP address.

It will allow any form of URL, but the path will be transformed by changing all special characters into +. For example, it will convert /players/guild/awesomeo/ into players+guild+awesomeo.json. The file itself will always be placed in the root of the data folder.

### Build

```bash
docker build -f examples/jc/Dockerfile -t westcoastcode-se/highway/examples_jc:latest .
docker run --rm -it -p 8080:8080 -v $(pwd)/examples/jc/data:/data westcoastcode-se/highway/examples_jc:latest
```

### Usage

#### GET

You can fetch a stored json entry by calling `localhost:8080` (or using your computer's IP address). The path after that
is then used as basis for where the object is stored on your machine.

For example

```bash
curl localhost:8080/players/existing_user
```

Will return the content of the file `data/players+existing_user.json`.

You can manually create or update files by creating files following this pattern.

If you request JSON content that does not exist, then a **404** will be returned:

```bash
curl localhost:8080/players/dont_exist -D -
HTTP/1.1 404 Not Found
Server: Highway 0.0.1
Content-Type: application/json
Content-Length: 0
```

#### PUT

You can also add or update json entries by sending a `PUT` to the server. For example:

```bash
curl http://127.0.0.1:8080/players/john_doe -XPUT -d '{"name":"John Doe"}'
```

This will add a file called `players+john_doe.json` in the data folder with the content `{"name":"John Doe"}`.

If the item already exist, then the previous value will be returned to the client

#### DELETE

You can also delete json entries by sending a DELETE request to the server. For example:

```bash
curl http://127.0.0.1:8080/players/john_doe -XDELETE
```

If the JSON entry exist then a **200** is returned with the removed content. Otherwise, a **404** will be returned.

The file itself will not be deleted. It will, instead, be renamed from `players+john_doe.json` into
`players+john_doe.json.trash`. This is so that we can manually undo the delete if we want to.

The only way the `trash` file is removed is if the item is added again and the that, in turn, is deleted

# License

The highway project is licensed under the MIT License

See the [LICENSE](LICENSE) file in the project root for license terms
