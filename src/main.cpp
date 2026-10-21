// main.cpp
//
// Entry point. Keeps one cluster in memory (shared by whoever opens the
// page) and decides what to do for each URL:
//
//   GET  /             manual cluster page
//   POST /add          add a server, then go back to /
//   POST /remove       remove a server, then go back to /
//   POST /setup        rebuild cluster with new settings, then go back to /
//   GET  /experiments  random runs (results are part of the same page)
//   GET  /notes        explanation page
//
// After a POST the answer is a redirect (303) to "/". That way pressing
// F5 in the browser just reloads the page instead of adding another server.

#include <iostream>
#include <cstdlib>

#include "web_server.h"
#include "cluster.h"
#include "pages.h"
#include "text_utils.h"

// ---- state of the manual playground ----
static ShardedCluster cluster;
static std::vector<OperationResult> history;

// A message to show once on the next page load ("Added node-5", or an error)
static std::string pendingMessage;
static bool pendingMessageIsError = false;

// The history chart stays readable by remembering at most 60 changes.
static const size_t MAX_HISTORY = 60;

static HttpResponse htmlPage(const std::string& body)
{
    HttpResponse response;
    response.statusCode = 200;
    response.contentType = "text/html; charset=utf-8";
    response.body = body;
    return response;
}

static HttpResponse redirectTo(const std::string& path)
{
    HttpResponse response;
    response.statusCode = 303;
    response.contentType = "text/plain";
    response.body = "";
    response.redirectTo = path;
    return response;
}

static HttpResponse notFound()
{
    HttpResponse response;
    response.statusCode = 404;
    response.contentType = "text/plain";
    response.body = "Not found";
    return response;
}

static void remember(const OperationResult& result)
{
    history.push_back(result);
    if (history.size() > MAX_HISTORY)
    {
        history.erase(history.begin());
    }
}

HttpResponse handleRequest(const HttpRequest& request)
{
    // ---- the main page ----
    if (request.path == "/" && request.method == "GET")
    {
        std::string page = buildClusterPage(cluster, history, pendingMessage, pendingMessageIsError);
        pendingMessage = "";   // show each message only once
        return htmlPage(page);
    }

    // ---- add a server ----
    if (request.path == "/add" && request.method == "POST")
    {
        std::string name = getParam(request, "name", "");
        if (name.empty())
        {
            name = cluster.makeNextNodeName();
        }

        OperationResult result;
        std::string error;
        if (cluster.addNode(name, result, error))
        {
            remember(result);
            pendingMessage = "Added " + name + ".";
            pendingMessageIsError = false;
        }
        else
        {
            pendingMessage = error;
            pendingMessageIsError = true;
        }
        return redirectTo("/");
    }

    // ---- remove a server ----
    if (request.path == "/remove" && request.method == "POST")
    {
        std::string name = getParam(request, "name", "");

        OperationResult result;
        std::string error;
        if (cluster.removeNode(name, result, error))
        {
            remember(result);
            pendingMessage = "Removed " + name + ".";
            pendingMessageIsError = false;
        }
        else
        {
            pendingMessage = error;
            pendingMessageIsError = true;
        }
        return redirectTo("/");
    }

    // ---- rebuild with new settings ----
    if (request.path == "/setup" && request.method == "POST")
    {
        int keys = textToInt(getParam(request, "keys", ""), 10000, 100, 200000);
        int vnodes = textToInt(getParam(request, "vnodes", ""), 50, 1, 500);
        int nodes = textToInt(getParam(request, "nodes", ""), 4, 1, 40);
        int seed = textToInt(getParam(request, "seed", ""), 42, 1, 99999);

        cluster.setup(keys, vnodes, nodes, seed);
        history.clear();

        pendingMessage = "Rebuilt: " + intToText(keys) + " keys, " + intToText(nodes) +
                         " servers, " + intToText(vnodes) + " vnodes each.";
        pendingMessageIsError = false;
        return redirectTo("/");
    }

    if (request.path == "/experiments")
    {
        return htmlPage(buildExperimentsPage(request));
    }

    if (request.path == "/notes")
    {
        return htmlPage(buildNotesPage());
    }

    return notFound();
}

int main(int argc, char* argv[])
{
    // Port can be given on the command line: sharding_demo 9000
    int port = 8080;
    if (argc > 1)
    {
        port = textToInt(argv[1], 8080, 1, 65535);
    }

    // Starting cluster: 10,000 keys, 4 servers, 20 vnodes each
    std::cout << "Building the starting cluster..." << std::endl;
    cluster.setup(10000, 20, 4, 42);

    if (!runServer(port))
    {
        return 1;
    }
    return 0;
}
