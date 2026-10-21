// metrics.h
//
// The numbers used to compare the two strategies.
//
// "owners" below is always a list where owners[i] is the server that holds
// records[i]. Comparing the owners list from before a change with the one
// from after the change shows exactly which keys had to move.

#ifndef METRICS_H
#define METRICS_H

#include <string>
#include <vector>

#include "dataset.h"

// One key that changed server, kept so the page can show real examples.
struct MovedKey
{
    std::string key;
    std::string fromNode;
    std::string toNode;
    int sizeKB;
};

// How much moved after one add/remove.
struct MoveSummary
{
    int totalKeys;
    int keysMoved;
    double percentKeysMoved;

    long long totalDataKB;
    long long dataMovedKB;

    // The first few keys that moved (not all of them, that could be 100k).
    std::vector<MovedKey> examples;
};

// How many keys / how much data one server holds.
struct NodeLoad
{
    std::string node;
    int keyCount;
    long long dataKB;
};

// How evenly the keys are spread over the servers.
struct LoadSummary
{
    std::vector<NodeLoad> perNode;

    double averageKeys;          // total keys / number of servers
    int mostKeys;                // keys on the busiest server
    int fewestKeys;              // keys on the least busy server

    // How many percent more keys the busiest server holds compared to the
    // average. 0% means perfectly even, 50% means it holds 1.5x the average.
    double busiestExtraPercent;
};

// Compares two owner lists and counts the moved keys and moved kilobytes.
MoveSummary compareOwners(const std::vector<std::string>& ownersBefore,
                          const std::vector<std::string>& ownersAfter,
                          const std::vector<DataRecord>& records,
                          int maxExamples);

// Counts keys and data per server and works out how balanced it is.
LoadSummary measureLoad(const std::vector<std::string>& owners,
                        const std::vector<std::string>& nodeNames,
                        const std::vector<DataRecord>& records);

#endif
