The goal is for hw (pronounced: highway) is to become a low-memory, high-performance HTTP framework for C applications
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
- [ ] ...

# Examples

## (hello_world) Hello World

A simple hello world example. Exposes http://127.0.0.1:8080

## (static) Static

A tiny static content server. Exposes on http://127.0.0.1:8080

## (boot) Boot

Usage:

```bash
Usage: static [data-dir] [max-threads] [read-timeout] [write-timeout]

  data-dir
```

A tiny rest server using Highway Boot. Exposes on http://127.0.0.1:8080

# License

The highway project is licensed under the MIT License

See the [LICENSE](LICENSE) file in the project root for license terms
