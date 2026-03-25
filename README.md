*This project has been created as part of the 42 curriculum by vpushkar, omizin and psmolin.*

# Webserv

## Description

Webserv is a lightweight HTTP/1.1 server written in C++17. It handles GET, POST, and DELETE requests, serves static files, supports file uploads, CGI execution (Python, Shell), cookie-based session management, and virtual hosting on multiple ports. All network I/O is non-blocking and driven by a single `poll()` loop.

## Instructions

### Build

```bash
make
```

### Run

```bash
./webserv [configuration_file]
```

If no configuration file is provided, `config/default.conf` is used by default.

### Configuration

The configuration file follows an NGINX-inspired syntax. Example:

```
server {
    listen 8080;
    host 0.0.0.0;
    server_name localhost;
    client_max_body_size 1048576;
    error_page 404 /errors/404.html;

    location / {
        root www;
        index index.html;
        methods GET POST;
        autoindex on;
    }

    location /uploads {
        root www/uploads;
        methods GET POST DELETE;
        upload_dir www/uploads;
    }

    location .py {
        cgi_path /usr/bin/python3;
        cgi_ext .py;
        methods GET POST;
    }
}
```

### Testing

```bash
make test          # Full test suite
make test-edge     # Edge case tests
```

## Resources

- [RFC 7230 HTTP/1.1: Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 7231 HTTP/1.1: Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231)
- [RFC 3875 CGI](https://datatracker.ietf.org/doc/html/rfc3875)

### AI Usage

AI tools were used for:
- Code review and identifying potential bugs (FD management, race conditions)
- Generating test scripts for edge cases and protocol compliance
- Debugging specific issues found by the school tester (413 errors, CGI handling)
- Explaining HTTP protocol nuances (Expect/100-continue, chunked transfer encoding)
