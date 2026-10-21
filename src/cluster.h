// cluster.h
//
// ShardedCluster runs ALL THREE strategies side by side on the SAME data and
// the SAME list of servers. Every time a server is added or removed, it asks
// each strategy "where does each key live now?" and compares with the answer
// from before the change.
//
// This is what the web page and the random experiments talk to.

#ifndef CLUSTER_H
#define CLUSTER_H

#include <string>
#include <vector>
#include <map>

#include "dataset.h"
#include "metrics.h"
#include "modulo_sharding.h"
#include "consistent_hashing.h"

// Everything measured for one add or remove.
struct OperationResult
{
    std::string label;          // short, for chart axes: "+node-5" / "-node-2"
    std::string description;    // longer, for tables: "Added node-5"
    int nodesBefore;
    int nodesAfter;

    MoveSummary modulo;         // what hash % N had to move
    MoveSummary consistent;     // what the ring had to move
    MoveSummary bounded;        // what the ring with load limit had to move

    // The smallest number of keys that MUST move to keep things balanced.
    //   adding:   the new server should end up with K / (N+1) keys
    //   removing: the removed server's ~K / N keys must go somewhere
    int idealKeysMoved;
    double idealPercent;
};

class ShardedCluster
{
public:
    ShardedCluster();

    // Throws everything away and starts again with fresh data and
    // `startNodes` servers called node-1, node-2, ...
    void setup(int keyCount, int vnodesPerNode, int startNodes, unsigned int seed);

    // Adds a server to both strategies and fills `result` with the numbers.
    // On failure returns false and puts the reason in `error`.
    bool addNode(const std::string& nodeName, OperationResult& result, std::string& error);

    // Removes a server from both strategies, same idea as addNode.
    bool removeNode(const std::string& nodeName, OperationResult& result, std::string& error);

    // Gives the next free automatic name, like "node-7".
    std::string makeNextNodeName();

    bool hasNode(const std::string& nodeName) const;

    // ---- read-only access for pages and charts ----
    const std::vector<std::string>& getNodes() const;
    const std::vector<DataRecord>& getRecords() const;
    const std::vector<std::string>& getModuloOwners() const;
    const std::vector<std::string>& getRingOwners() const;
    const std::vector<std::string>& getBoundedOwners() const;
    const ConsistentHashRing& getRing() const;
    int getVnodesPerNode() const;
    unsigned int getSeed() const;

    // Each server keeps the same colour for its whole life, so the charts
    // don't reshuffle colours when someone in the middle is removed.
    std::string getNodeColor(const std::string& nodeName) const;
    std::map<std::string, std::string> getAllNodeColors() const;

private:
    // Asks each strategy for the owner of every record.
    void computeModuloOwners(std::vector<std::string>& owners) const;
    void computeRingOwners(std::vector<std::string>& owners) const;

    // Recomputes the owners of all three strategies.
    void computeAllOwners();

    // Picks a colour for a newly added server.
    void assignColor(const std::string& nodeName);

    std::vector<DataRecord> records;

    ModuloSharding modulo;
    ConsistentHashRing ring;

    // moduloOwners[i] / ringOwners[i] / boundedOwners[i] = server that
    // currently holds records[i] in each strategy
    std::vector<std::string> moduloOwners;
    std::vector<std::string> ringOwners;
    std::vector<std::string> boundedOwners;

    std::map<std::string, int> colorIndexOfNode;
    int nextColorIndex;
    int nameCounter;
    unsigned int seed;
};

#endif
