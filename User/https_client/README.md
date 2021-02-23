# https_client
HTTP/HTTPS REST Client C Library

This library is a tiny https client library. it use only small memory(default read buffer size(H_READ_SIZE) is 2048).
This library can be easily applied in your embedded system because written only in C language.

It use the mbedTLS ssl/tls library(https://tls.mbed.org/). You must include the mbedTLS library.

Supporting specifications are as follows.

- Support the HTTPS 1.1 Keep-Alive connection.
- Support the chunked-encoding.
- Support the HTTP GET and POST method.

Enjoy.
