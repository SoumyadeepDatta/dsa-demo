// metrics.cpp
//
// Plain counting. Every key is checked one by one to see whether its
// server changed.

#include "metrics.h"

#include <map>

MoveSummary compareOwners(const std::vector<std::string>& ownersBefore,
                          const std::vector<std::string>& ownersAfter,
                          const std::vector<DataRecord>& records,
                          int maxExamples)
{
    MoveSummary summary;
    summary.totalKeys = (int)records.size();
    summary.keysMoved = 0;
    summary.totalDataKB = 0;
    summary.dataMovedKB = 0;

    for (size_t i = 0; i < records.size(); i++)
    {
        summary.totalDataKB += records[i].sizeKB;

        // Same server as before? Then nothing has to be copied.
        if (ownersBefore[i] == ownersAfter[i])
        {
            continue;
        }

        // Different server: this row has to be sent over the network.
        summary.keysMoved++;
        summary.dataMovedKB += records[i].sizeKB;

        if ((int)summary.examples.size() < maxExamples)
        {
            MovedKey moved;
            moved.key = records[i].key;
            moved.fromNode = ownersBefore[i];
            moved.toNode = ownersAfter[i];
            moved.sizeKB = records[i].sizeKB;
            summary.examples.push_back(moved);
        }
    }

    summary.percentKeysMoved = 0.0;
    if (summary.totalKeys > 0)
    {
        summary.percentKeysMoved = 100.0 * summary.keysMoved / summary.totalKeys;
    }

    return summary;
}

LoadSummary measureLoad(const std::vector<std::string>& owners,
                        const std::vector<std::string>& nodeNames,
                        const std::vector<DataRecord>& records)
{
    LoadSummary summary;

    // Start every server at zero. The map gives the position of a server
    // inside perNode quickly, by its name.
    std::map<std::string, int> slotOfNode;
    for (size_t i = 0; i < nodeNames.size(); i++)
    {
        NodeLoad load;
        load.node = nodeNames[i];
        load.keyCount = 0;
        load.dataKB = 0;

        slotOfNode[nodeNames[i]] = (int)i;
        summary.perNode.push_back(load);
    }

    // Count every record towards its server.
    for (size_t i = 0; i < records.size(); i++)
    {
        std::map<std::string, int>::iterator slot = slotOfNode.find(owners[i]);
        if (slot != slotOfNode.end())
        {
            summary.perNode[slot->second].keyCount++;
            summary.perNode[slot->second].dataKB += records[i].sizeKB;
        }
    }

    summary.averageKeys = 0;
    summary.mostKeys = 0;
    summary.fewestKeys = 0;
    summary.busiestExtraPercent = 0;

    int nodeCount = (int)nodeNames.size();
    if (nodeCount == 0)
    {
        return summary;
    }

    // Average keys per server
    summary.averageKeys = (double)records.size() / nodeCount;

    // Find the busiest and the least busy server
    summary.mostKeys = summary.perNode[0].keyCount;
    summary.fewestKeys = summary.perNode[0].keyCount;
    for (int i = 1; i < nodeCount; i++)
    {
        if (summary.perNode[i].keyCount > summary.mostKeys)
        {
            summary.mostKeys = summary.perNode[i].keyCount;
        }
        if (summary.perNode[i].keyCount < summary.fewestKeys)
        {
            summary.fewestKeys = summary.perNode[i].keyCount;
        }
    }

    // Example: average 2000, busiest 2600 -> busiest has 30% extra
    if (summary.averageKeys > 0)
    {
        summary.busiestExtraPercent = 100.0 * (summary.mostKeys - summary.averageKeys) / summary.averageKeys;
    }

    return summary;
}
