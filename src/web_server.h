// web_server.h
//
// A very small HTTP server written straight on top of sockets.
//
// It is NOT meant for real traffic: it handles one browser request at a
// time, understands only GET and POST with simple form data, and always
// closes the connection after answering. For a local demo that is enough,
// and it means the project has zero dependencies besides the C++ standard
// library and the operating system's socket API.

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <string>
#include <map>

// What the browser asked for.
struct HttpRequest
{
    std::string method;                         // "GET" or "POST"
    std::string path;                           // "/experiments" (without ?query)
    std::map<std::string, std::string> params;  // form fields + query string values
};

// What is sent back to the browser.
struct HttpResponse
{
    int statusCode;              // 200 = ok, 303 = redirect, 404 = not found
    std::string contentType;     // usually "text/html; charset=utf-8"
    std::string body;            // the page itself
    std::string redirectTo;      // only used when statusCode is 303
};

// Gets a parameter from the request, or `fallback` if the browser didn't send it.
std::string getParam(const HttpRequest& request, const std::string& name, const std::string& fallback);

// The function that decides what to do with each request. web_server.cpp
// calls it, main.cpp writes it.
HttpResponse handleRequest(const HttpRequest& request);

// Opens the port and serves requests forever. Returns false if the port
// could not be opened (for example, it is already in use).
bool runServer(int port);

#endif
