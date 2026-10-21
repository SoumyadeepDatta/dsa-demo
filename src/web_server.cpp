// web_server.cpp
//
// How a browser talks to a web server, in short:
//
//   1. the browser opens a TCP connection to the server's port
//   2. it sends text like:
//          POST /add HTTP/1.1
//          Host: localhost:8080
//          Content-Length: 11
//          (empty line)
//          name=node-9
//   3. the server answers with text like:
//          HTTP/1.1 200 OK
//          Content-Type: text/html
//          Content-Length: 1234
//          (empty line)
//          <html>...</html>
//   4. the server closes the connection
//
// Windows and Linux/macOS have almost the same socket functions, with a few
// different names. The #ifdef block below hides those differences.

#include "web_server.h"
#include "text_utils.h"

#include <iostream>
#include <sstream>
#include <cstdlib>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <unistd.h>
    typedef int SOCKET;
    #define INVALID_SOCKET (-1)
    #define closesocket close
#endif

std::string getParam(const HttpRequest& request, const std::string& name, const std::string& fallback)
{
    std::map<std::string, std::string>::const_iterator it = request.params.find(name);
    if (it == request.params.end())
    {
        return fallback;
    }
    return it->second;
}

// Splits "a=1&b=hello+world" into params["a"]="1", params["b"]="hello world"
static void parseFormData(const std::string& text, std::map<std::string, std::string>& params)
{
    std::stringstream stream(text);
    std::string pair;

    while (std::getline(stream, pair, '&'))
    {
        if (pair.empty())
        {
            continue;
        }

        size_t equalsSign = pair.find('=');
        if (equalsSign == std::string::npos)
        {
            params[urlDecode(pair)] = "";
        }
        else
        {
            std::string name = urlDecode(pair.substr(0, equalsSign));
            std::string value = urlDecode(pair.substr(equalsSign + 1));
            params[name] = value;
        }
    }
}

// Reads one whole request from the socket. Returns false if the browser
// hung up or sent something that can't be understood.
static bool readRequest(SOCKET client, HttpRequest& request)
{
    std::string data;
    char buffer[4096];

    // Keep reading until all the headers have arrived (they end with an empty line)
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos)
    {
        int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0)
        {
            return false;
        }
        data.append(buffer, received);
        headerEnd = data.find("\r\n\r\n");

        if (data.size() > 64 * 1024)
        {
            return false;   // nobody needs headers this big
        }
    }

    std::string headers = data.substr(0, headerEnd);
    std::string body = data.substr(headerEnd + 4);

    // First line: "GET /experiments?run=vnodes HTTP/1.1"
    std::stringstream firstLine(headers.substr(0, headers.find("\r\n")));
    std::string fullPath;
    firstLine >> request.method >> fullPath;

    if (request.method.empty() || fullPath.empty())
    {
        return false;
    }

    // Look for "Content-Length:" to know how long the body is. Header names
    // are case-insensitive, so search in a lower-case copy.
    std::string lowerHeaders = headers;
    for (size_t i = 0; i < lowerHeaders.size(); i++)
    {
        if (lowerHeaders[i] >= 'A' && lowerHeaders[i] <= 'Z')
        {
            lowerHeaders[i] = lowerHeaders[i] - 'A' + 'a';
        }
    }

    size_t bodyLength = 0;
    size_t lengthPos = lowerHeaders.find("content-length:");
    if (lengthPos != std::string::npos)
    {
        bodyLength = std::atoi(lowerHeaders.c_str() + lengthPos + 15);
    }
    if (bodyLength > 1024 * 1024)
    {
        return false;
    }

    // The body may not have arrived completely with the headers.
    while (body.size() < bodyLength)
    {
        int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0)
        {
            break;
        }
        body.append(buffer, received);
    }

    // Split "/experiments?run=vnodes" into path and query string
    size_t questionMark = fullPath.find('?');
    if (questionMark == std::string::npos)
    {
        request.path = fullPath;
    }
    else
    {
        request.path = fullPath.substr(0, questionMark);
        parseFormData(fullPath.substr(questionMark + 1), request.params);
    }

    // Form fields sent with POST are in the body, same a=1&b=2 format
    if (request.method == "POST")
    {
        parseFormData(body, request.params);
    }

    return true;
}

// Turns an HttpResponse into the text format the browser expects and sends it.
static void sendResponse(SOCKET client, const HttpResponse& response)
{
    std::string statusText = "OK";
    if (response.statusCode == 303) statusText = "See Other";
    if (response.statusCode == 404) statusText = "Not Found";

    std::ostringstream head;
    head << "HTTP/1.1 " << response.statusCode << " " << statusText << "\r\n";
    head << "Content-Type: " << response.contentType << "\r\n";
    head << "Content-Length: " << response.body.size() << "\r\n";
    head << "Cache-Control: no-store\r\n";
    if (response.statusCode == 303)
    {
        head << "Location: " << response.redirectTo << "\r\n";
    }
    head << "Connection: close\r\n\r\n";

    std::string everything = head.str() + response.body;

    // send() may not take everything at once, so loop until it's all gone
    size_t sent = 0;
    while (sent < everything.size())
    {
        int result = send(client, everything.c_str() + sent, (int)(everything.size() - sent), 0);
        if (result <= 0)
        {
            break;
        }
        sent += result;
    }
}

// Browsers sometimes open a connection "just in case" and send nothing.
// Since this server handles one connection at a time, that would freeze it.
// A 3 second receive timeout makes sure such connections are dropped.
static void setReceiveTimeout(SOCKET client)
{
#ifdef _WIN32
    DWORD milliseconds = 3000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&milliseconds, sizeof(milliseconds));
#else
    struct timeval wait;
    wait.tv_sec = 3;
    wait.tv_usec = 0;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&wait, sizeof(wait));
#endif
}

bool runServer(int port)
{
#ifdef _WIN32
    // Windows needs the socket library to be started before use
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cout << "Could not start Winsock." << std::endl;
        return false;
    }
#endif

    // 1. create a TCP socket
    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET)
    {
        std::cout << "Could not create a socket." << std::endl;
        return false;
    }

    // On Linux/macOS this allows restarting the program right away on the
    // same port. It is skipped on Windows, because there the same option
    // lets two programs share one port, so a busy port would not be noticed.
#ifndef _WIN32
    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#endif

    // 2. attach it to localhost:port. Only 127.0.0.1 is used so the
    //    demo is not reachable from other machines.
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(0x7F000001);   // 127.0.0.1

    if (bind(listener, (sockaddr*)&address, sizeof(address)) != 0)
    {
        std::cout << "Port " << port << " is busy. Try: sharding_demo " << port + 1 << std::endl;
        closesocket(listener);
        return false;
    }

    // 3. start accepting connections
    if (listen(listener, 16) != 0)
    {
        std::cout << "listen() failed." << std::endl;
        closesocket(listener);
        return false;
    }

    std::cout << "Server running. Open http://localhost:" << port << " in a browser." << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    // 4. serve forever, one request at a time
    while (true)
    {
        SOCKET client = accept(listener, NULL, NULL);
        if (client == INVALID_SOCKET)
        {
            continue;
        }

        setReceiveTimeout(client);

        HttpRequest request;
        if (readRequest(client, request))
        {
            std::cout << request.method << " " << request.path << std::endl;
            HttpResponse response = handleRequest(request);
            sendResponse(client, response);
        }

        closesocket(client);
    }

    return true;
}
