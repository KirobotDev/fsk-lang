
#ifdef _WIN32
    #if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
        #define _CRT_SECURE_NO_WARNINGS 
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <fcntl.h>
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #pragma comment( lib, "ws2_32" )
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/types.h>
    #include <io.h>
    #ifndef _SSIZE_T_DEFINED
        typedef int ssize_t;
        #define _SSIZE_T_DEFINED
    #endif
    #ifndef _SOCKET_T_DEFINED
        typedef SOCKET socket_t;
        #define _SOCKET_T_DEFINED
    #endif
    #ifndef snprintf
        #define snprintf _snprintf_s
    #endif
    #if _MSC_VER >=1600
        #include <stdint.h>
    #else
        typedef __int8 int8_t;
        typedef unsigned __int8 uint8_t;
        typedef __int32 int32_t;
        typedef unsigned __int32 uint32_t;
        typedef __int64 int64_t;
        typedef unsigned __int64 uint64_t;
    #endif
    #define socketerrno WSAGetLastError()
    #define SOCKET_EAGAIN_EINPROGRESS WSAEINPROGRESS
    #define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
#else
    #include <fcntl.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <stdint.h>
    #ifndef _SOCKET_T_DEFINED
        typedef int socket_t;
        #define _SOCKET_T_DEFINED
    #endif
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET (-1)
    #endif
    #ifndef SOCKET_ERROR
        #define SOCKET_ERROR   (-1)
    #endif
    #define closesocket(s) ::close(s)
    #include <errno.h>
    #define socketerrno errno
    #define SOCKET_EAGAIN_EINPROGRESS EAGAIN
    #define SOCKET_EWOULDBLOCK EWOULDBLOCK
#endif

#include <vector>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "easywsclient.hpp"

using easywsclient::Callback_Imp;
using easywsclient::BytesCallback_Imp;

namespace { 

socket_t hostname_connect(const std::string& hostname, int port) {
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *p;
    int ret;
    socket_t sockfd = INVALID_SOCKET;
    char sport[16];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(sport, 16, "%d", port);
    if ((ret = getaddrinfo(hostname.c_str(), sport, &hints, &result)) != 0)
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
      return 1;
    }
    for(p = result; p != NULL; p = p->ai_next)
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == INVALID_SOCKET) { continue; }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) != SOCKET_ERROR) {
            break;
        }
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    return sockfd;
}


class _DummyWebSocket : public easywsclient::WebSocket
{
  public:
    void poll(int timeout) { }
    void send(const std::string& message) { }
    void sendBinary(const std::string& message) { }
    void sendBinary(const std::vector<uint8_t>& message) { }
    void sendPing() { }
    void close() { } 
    readyStateValues getReadyState() const { return CLOSED; }
    void _dispatch(Callback_Imp & callable) { }
    void _dispatchBinary(BytesCallback_Imp& callable) { }
};


class _RealWebSocket : public easywsclient::WebSocket
{
  public:
    struct wsheader_type {
        unsigned header_size;
        bool fin;
        bool mask;
        enum opcode_type {
            CONTINUATION = 0x0,
            TEXT_FRAME = 0x1,
            BINARY_FRAME = 0x2,
            CLOSE = 8,
            PING = 9,
            PONG = 0xa,
        } opcode;
        int N0;
        uint64_t N;
        uint8_t masking_key[4];
    };

    std::vector<uint8_t> rxbuf;
    std::vector<uint8_t> txbuf;
    std::vector<uint8_t> receivedData;

    socket_t sockfd;
    readyStateValues readyState;
    bool useMask;
    bool isRxBad;
    
    SSL_CTX *ctx = nullptr;
    SSL *ssl = nullptr;

    _RealWebSocket(socket_t sockfd, bool useMask, SSL *ssl = nullptr, SSL_CTX *ctx = nullptr)
            : sockfd(sockfd)
            , readyState(OPEN)
            , useMask(useMask)
            , isRxBad(false)
            , ssl(ssl)
            , ctx(ctx) {
    }
    
    ~_RealWebSocket() {
        if (ssl) SSL_free(ssl);
        if (ctx) SSL_CTX_free(ctx); 
    }
    
    ssize_t _recv(void *buf, size_t len) {
        if (ssl) {
            return SSL_read(ssl, buf, len);
        } else {
            return recv(sockfd, (char*)buf, len, 0);
        }
    }
    
    ssize_t _send(const void *buf, size_t len) {
        if (ssl) {
            return SSL_write(ssl, buf, len);
        } else {
            return ::send(sockfd, (char*)buf, len, 0);
        }
    }

    readyStateValues getReadyState() const {
      return readyState;
    }

    void poll(int timeout) { 
        if (readyState == CLOSED) {
            if (timeout > 0) {
                timeval tv = { timeout/1000, (timeout%1000) * 1000 };
                select(0, NULL, NULL, NULL, &tv);
            }
            return;
        }
        if (timeout != 0) {
            fd_set rfds;
            fd_set wfds;
            timeval tv = { timeout/1000, (timeout%1000) * 1000 };
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_SET(sockfd, &rfds);
            if (txbuf.size()) { FD_SET(sockfd, &wfds); }
            select(sockfd + 1, &rfds, &wfds, 0, timeout > 0 ? &tv : 0);
        }
        while (true) {
            int N = rxbuf.size();
            ssize_t ret;
            rxbuf.resize(N + 1500);
            ret = _recv((char*)&rxbuf[0] + N, 1500); 
            if (false) { }
            else if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK || socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
                rxbuf.resize(N);
                break;
            }
            else if (ret <= 0) {
                if (ssl && ret < 0) {
                     int err = SSL_get_error(ssl, ret);
                     if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                         rxbuf.resize(N);
                         break;
                     }
                }
                rxbuf.resize(N);
                closesocket(sockfd);
                readyState = CLOSED;
                fputs(ret < 0 ? "Connection error!\n" : "Connection closed!\n", stderr);
                break;
            }
            else {
                rxbuf.resize(N + ret);
            }
        }
        while (txbuf.size()) {
            int ret = _send((char*)&txbuf[0], txbuf.size()); 
            if (false) { } 
            else if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK || socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
                break;
            }
            else if (ret <= 0) {
                if (ssl && ret < 0) {
                     int err = SSL_get_error(ssl, ret);
                     if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                         break;
                     }
                }
                closesocket(sockfd);
                readyState = CLOSED;
                fputs(ret < 0 ? "Connection error!\n" : "Connection closed!\n", stderr);
                break;
            }
            else {
                txbuf.erase(txbuf.begin(), txbuf.begin() + ret);
            }
        }
        if (!txbuf.size() && readyState == CLOSING) {
            closesocket(sockfd);
            readyState = CLOSED;
        }
    }

     virtual void _dispatch(Callback_Imp & callable) {
        struct CallbackAdapter : public BytesCallback_Imp
        {
            Callback_Imp& callable;
            CallbackAdapter(Callback_Imp& callable) : callable(callable) { }
            void operator()(const std::vector<uint8_t>& message) {
                std::string stringMessage(message.begin(), message.end());
                callable(stringMessage);
            }
        };
        CallbackAdapter bytesCallback(callable);
        _dispatchBinary(bytesCallback);
    }

    virtual void _dispatchBinary(BytesCallback_Imp & callable) {
        if (isRxBad) { return; }
        while (true) {
            wsheader_type ws;
            if (rxbuf.size() < 2) { return; }
            const uint8_t * data = (uint8_t *) &rxbuf[0]; 
            ws.fin = (data[0] & 0x80) == 0x80;
            ws.opcode = (wsheader_type::opcode_type) (data[0] & 0x0f);
            ws.mask = (data[1] & 0x80) == 0x80;
            ws.N0 = (data[1] & 0x7f);
            ws.header_size = 2 + (ws.N0 == 126? 2 : 0) + (ws.N0 == 127? 8 : 0) + (ws.mask? 4 : 0);
            if (rxbuf.size() < ws.header_size) { return; }
            int i = 0;
            if (ws.N0 < 126) {
                ws.N = ws.N0;
                i = 2;
            }
            else if (ws.N0 == 126) {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 8;
                ws.N |= ((uint64_t) data[3]) << 0;
                i = 4;
            }
            else if (ws.N0 == 127) {
                ws.N = 0;
                 ws.N |= ((uint64_t) data[2]) << 56;
                 ws.N |= ((uint64_t) data[3]) << 48;
                 ws.N |= ((uint64_t) data[4]) << 40;
                 ws.N |= ((uint64_t) data[5]) << 32;
                 ws.N |= ((uint64_t) data[6]) << 24;
                 ws.N |= ((uint64_t) data[7]) << 16;
                 ws.N |= ((uint64_t) data[8]) << 8;
                 ws.N |= ((uint64_t) data[9]) << 0;
                 i = 10;
                if (ws.N & 0x8000000000000000ull) {
                    isRxBad = true;
                    fprintf(stderr, "ERROR: Frame has invalid frame length. Closing.\n");
                    close();
                    return;
                }
            }
            if (ws.mask) {
                ws.masking_key[0] = ((uint8_t) data[i+0]) << 0;
                ws.masking_key[1] = ((uint8_t) data[i+1]) << 0;
                ws.masking_key[2] = ((uint8_t) data[i+2]) << 0;
                ws.masking_key[3] = ((uint8_t) data[i+3]) << 0;
            }
            else {
                ws.masking_key[0] = 0; ws.masking_key[1] = 0; ws.masking_key[2] = 0; ws.masking_key[3] = 0;
            }

            if (rxbuf.size() < ws.header_size+ws.N) { return; }

            if (false) { }
            else if (
                   ws.opcode == wsheader_type::TEXT_FRAME 
                || ws.opcode == wsheader_type::BINARY_FRAME
                || ws.opcode == wsheader_type::CONTINUATION
            ) {
                if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { rxbuf[i+ws.header_size] ^= ws.masking_key[i&0x3]; } }
                receivedData.insert(receivedData.end(), rxbuf.begin()+ws.header_size, rxbuf.begin()+ws.header_size+(size_t)ws.N);
                if (ws.fin) {
                    callable((const std::vector<uint8_t>) receivedData);
                    receivedData.erase(receivedData.begin(), receivedData.end());
                    std::vector<uint8_t> ().swap(receivedData);
                }
            }
            else if (ws.opcode == wsheader_type::PING) {
                if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { rxbuf[i+ws.header_size] ^= ws.masking_key[i&0x3]; } }
                std::string data(rxbuf.begin()+ws.header_size, rxbuf.begin()+ws.header_size+(size_t)ws.N);
                sendData(wsheader_type::PONG, data.size(), data.begin(), data.end());
            }
            else if (ws.opcode == wsheader_type::PONG) { }
            else if (ws.opcode == wsheader_type::CLOSE) { close(); }
            else { fprintf(stderr, "ERROR: Got unexpected WebSocket message.\n"); close(); }

            rxbuf.erase(rxbuf.begin(), rxbuf.begin() + ws.header_size+(size_t)ws.N);
        }
    }

    void sendPing() {
        std::string empty;
        sendData(wsheader_type::PING, empty.size(), empty.begin(), empty.end());
    }

    void send(const std::string& message) {
        sendData(wsheader_type::TEXT_FRAME, message.size(), message.begin(), message.end());
    }

    void sendBinary(const std::string& message) {
        sendData(wsheader_type::BINARY_FRAME, message.size(), message.begin(), message.end());
    }

    void sendBinary(const std::vector<uint8_t>& message) {
        sendData(wsheader_type::BINARY_FRAME, message.size(), message.begin(), message.end());
    }

    template<class Iterator>
    void sendData(wsheader_type::opcode_type type, uint64_t message_size, Iterator message_begin, Iterator message_end) {
        const uint8_t masking_key[4] = { 0x12, 0x34, 0x56, 0x78 };
        if (readyState == CLOSING || readyState == CLOSED) { return; }
        std::vector<uint8_t> header;
        header.assign(2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);
        header[0] = 0x80 | type;
        if (false) { }
        else if (message_size < 126) {
            header[1] = (message_size & 0xff) | (useMask ? 0x80 : 0);
            if (useMask) {
                header[2] = masking_key[0]; header[3] = masking_key[1]; header[4] = masking_key[2]; header[5] = masking_key[3];
            }
        }
        else if (message_size < 65536) {
            header[1] = 126 | (useMask ? 0x80 : 0);
            header[2] = (message_size >> 8) & 0xff; header[3] = (message_size >> 0) & 0xff;
            if (useMask) {
                header[4] = masking_key[0]; header[5] = masking_key[1]; header[6] = masking_key[2]; header[7] = masking_key[3];
            }
        }
        else { 
            header[1] = 127 | (useMask ? 0x80 : 0);
            header[2] = (message_size >> 56) & 0xff; header[3] = (message_size >> 48) & 0xff;
            header[4] = (message_size >> 40) & 0xff; header[5] = (message_size >> 32) & 0xff;
            header[6] = (message_size >> 24) & 0xff; header[7] = (message_size >> 16) & 0xff;
            header[8] = (message_size >>  8) & 0xff; header[9] = (message_size >>  0) & 0xff;
            if (useMask) {
                header[10] = masking_key[0]; header[11] = masking_key[1]; header[12] = masking_key[2]; header[13] = masking_key[3];
            }
        }
        txbuf.insert(txbuf.end(), header.begin(), header.end());
        txbuf.insert(txbuf.end(), message_begin, message_end);
        if (useMask) {
            size_t message_offset = txbuf.size() - message_size;
            for (size_t i = 0; i != message_size; ++i) {
                txbuf[message_offset + i] ^= masking_key[i&0x3];
            }
        }
    }

    void close() {
        if(readyState == CLOSING || readyState == CLOSED) { return; }
        readyState = CLOSING;
        uint8_t closeFrame[6] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00}; 
        std::vector<uint8_t> header(closeFrame, closeFrame+6);
        txbuf.insert(txbuf.end(), header.begin(), header.end());
    }

};


easywsclient::WebSocket::pointer from_url(const std::string& url, bool useMask, const std::string& origin) {
    char host[512];
    int port;
    char path[512];
    bool useSSL = false;
    
    if (url.size() >= 512) { return NULL; }
    
    if (sscanf(url.c_str(), "ws://%[^:/]:%d/%s", host, &port, path) == 3) { }
    else if (sscanf(url.c_str(), "ws://%[^:/]/%s", host, path) == 2) { port = 80; }
    else if (sscanf(url.c_str(), "ws://%[^:/]:%d", host, &port) == 2) { path[0] = '\0'; }
    else if (sscanf(url.c_str(), "ws://%[^:/]", host) == 1) { port = 80; path[0] = '\0'; }
    else if (sscanf(url.c_str(), "wss://%[^:/]:%d/%s", host, &port, path) == 3) { useSSL = true; }
    else if (sscanf(url.c_str(), "wss://%[^:/]/%s", host, path) == 2) { port = 443; useSSL = true; }
    else if (sscanf(url.c_str(), "wss://%[^:/]:%d", host, &port) == 2) { path[0] = '\0'; useSSL = true; }
    else if (sscanf(url.c_str(), "wss://%[^:/]", host) == 1) { port = 443; path[0] = '\0'; useSSL = true; }
    else {
        fprintf(stderr, "ERROR: Could not parse WebSocket url: %s\n", url.c_str());
        return NULL;
    }
    
    socket_t sockfd = hostname_connect(host, port);
    if (sockfd == INVALID_SOCKET) {
        fprintf(stderr, "Unable to connect to %s:%d\n", host, port);
        return NULL;
    }

    SSL_CTX *ctx = nullptr;
    SSL *ssl = nullptr;

    if (useSSL) {
        SSL_library_init();
        SSL_load_error_strings();
        ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) { fprintf(stderr, "SSL_CTX_new failed\n"); closesocket(sockfd); return NULL; }
        ssl = SSL_new(ctx);
        if (!ssl) { fprintf(stderr, "SSL_new failed\n"); SSL_CTX_free(ctx); closesocket(sockfd); return NULL; }
        
        SSL_set_fd(ssl, sockfd);
        SSL_set_tlsext_host_name(ssl, host);
        if (SSL_connect(ssl) != 1) {
            fprintf(stderr, "SSL_connect failed\n");
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            closesocket(sockfd);
            return NULL;
        }
    }
    
    {
        char line[1024];
        int status;
        int i;
        
        auto my_send = [&](const char* data) {
            if (useSSL) SSL_write(ssl, data, strlen(data));
            else ::send(sockfd, data, strlen(data), 0);
        };
        auto my_recv = [&](char* buf, int len) {
             if (useSSL) return SSL_read(ssl, buf, len);
             return (int)::recv(sockfd, buf, len, 0);
        };

        snprintf(line, 1024, "GET /%s HTTP/1.1\r\n", path); my_send(line);
        if ((port == 80 && !useSSL) || (port == 443 && useSSL)) {
            snprintf(line, 1024, "Host: %s\r\n", host); my_send(line);
        }
        else {
            snprintf(line, 1024, "Host: %s:%d\r\n", host, port); my_send(line);
        }
        snprintf(line, 1024, "Upgrade: websocket\r\n"); my_send(line);
        snprintf(line, 1024, "Connection: Upgrade\r\n"); my_send(line);
        if (!origin.empty()) {
            snprintf(line, 1024, "Origin: %s\r\n", origin.c_str()); my_send(line);
        }
        snprintf(line, 1024, "Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"); my_send(line);
        snprintf(line, 1024, "Sec-WebSocket-Version: 13\r\n"); my_send(line);
        snprintf(line, 1024, "\r\n"); my_send(line);
        
        for (i = 0; i < 2 || (i < 1023 && line[i-2] != '\r' && line[i-1] != '\n'); ++i) { 
            if (my_recv(line+i, 1) <= 0) { return NULL; } 
        }
        line[i] = 0;
        
        if (i == 1023) { fprintf(stderr, "ERROR: Got invalid status line connecting to: %s\n", url.c_str()); return NULL; }
        if (sscanf(line, "HTTP/1.1 %d", &status) != 1 || status != 101) { fprintf(stderr, "ERROR: Got bad status connecting to %s: %s", url.c_str(), line); return NULL; }
        
        while (true) {
            for (i = 0; i < 2 || (i < 1023 && line[i-2] != '\r' && line[i-1] != '\n'); ++i) { 
                if (my_recv(line+i, 1) <= 0) { return NULL; } 
            }
            if (line[0] == '\r' && line[1] == '\n') { break; }
        }
    }
    
    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(flag));
#ifdef _WIN32
    u_long on = 1;
    ioctlsocket(sockfd, FIONBIO, &on);
#else
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
#endif
    return easywsclient::WebSocket::pointer(new _RealWebSocket(sockfd, useMask, ssl, ctx));
}

} 

namespace easywsclient {
WebSocket::pointer WebSocket::create_dummy() {
    static pointer dummy = pointer(new _DummyWebSocket);
    return dummy;
}
WebSocket::pointer WebSocket::from_url(const std::string& url, const std::string& origin) {
    return ::from_url(url, true, origin);
}
WebSocket::pointer WebSocket::from_url_no_mask(const std::string& url, const std::string& origin) {
    return ::from_url(url, false, origin);
}
}