// pages.h
//
// Functions that build the HTML pages. Each one returns the full page as a
// string, which main.cpp then hands to the web server.

#ifndef PAGES_H
#define PAGES_H

#include <string>
#include <vector>

#include "cluster.h"
#include "web_server.h"

// "/" - the manual playground: add/remove servers and see what moved.
std::string buildClusterPage(const ShardedCluster& cluster,
                             const std::vector<OperationResult>& history,
                             const std::string& message,
                             bool messageIsError);

// "/experiments" - the random / automatic runs.
std::string buildExperimentsPage(const HttpRequest& request);

// "/notes" - short explanation of both algorithms.
std::string buildNotesPage();

#endif
