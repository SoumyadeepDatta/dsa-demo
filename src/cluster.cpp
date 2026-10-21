// cluster.cpp
//
// The add/remove flow is the same for both operations, and every step is
// done for all three strategies:
//
//   1. remember who owned every key   (ownersBefore)
//   2. change the server list (modulo list and ring)
//   3. ask again who owns every key   (ownersAfter)
//   4. compare before/after -> keys moved, data moved
//
// Doing the full recompute is slow compared to a real database (which would
// only scan affected ranges) but it is the most honest way to measure: no
// shortcuts, every single key is counted.

#include "cluster.h"
#include "text_utils.h"
#include "bounded_hashing.h"

// Colours for servers. Hand picked so neighbours are easy to tell apart.
static const char* NODE_COLORS[12] = {
    "#4e79a7", "#f28e2b", "#59a14f", "#e15759", "#76b7b2", "#edc948",
    "#b07aa1", "#ff9da7", "#9c755f", "#1b9e77", "#7570b3", "#a0a0a0"
};

// How many moved keys are kept as examples for the page.
static const int EXAMPLES_TO_KEEP = 8;

ShardedCluster::ShardedCluster()
{
    nextColorIndex = 0;
    nameCounter = 0;
    seed = 1;
}

void ShardedCluster::setup(int keyCount, int vnodesPerNode, int startNodes, unsigned int newSeed)
{
    seed = newSeed;
    records = generateDataset(keyCount, seed);

    modulo.clear();
    ring.clear();
    ring.setVirtualNodesPerNode(vnodesPerNode);

    colorIndexOfNode.clear();
    nextColorIndex = 0;
    nameCounter = 0;

    for (int i = 0; i < startNodes; i++)
    {
        std::string name = makeNextNodeName();
        modulo.addNode(name);
        ring.addNode(name);
        assignColor(name);
    }

    computeAllOwners();
}

std::string ShardedCluster::makeNextNodeName()
{
    // Keep counting up until a name is found that nobody uses yet (the user may
    // have typed "node-5" by hand earlier).
    std::string name;
    do
    {
        nameCounter++;
        name = "node-" + intToText(nameCounter);
    } while (hasNode(name));

    return name;
}

bool ShardedCluster::hasNode(const std::string& nodeName) const
{
    const std::vector<std::string>& nodes = modulo.getNodes();
    for (size_t i = 0; i < nodes.size(); i++)
    {
        if (nodes[i] == nodeName)
        {
            return true;
        }
    }
    return false;
}

// Small check for names typed on the web page. Only letters, digits,
// '-' and '_' are allowed, to keep things readable on the charts.
static bool isValidNodeName(const std::string& name)
{
    if (name.empty() || name.size() > 20)
    {
        return false;
    }

    for (size_t i = 0; i < name.size(); i++)
    {
        char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok)
        {
            return false;
        }
    }
    return true;
}

bool ShardedCluster::addNode(const std::string& nodeName, OperationResult& result, std::string& error)
{
    if (!isValidNodeName(nodeName))
    {
        error = "Server names can only use letters, digits, - and _ (max 20 characters).";
        return false;
    }
    if (hasNode(nodeName))
    {
        error = "There is already a server called " + nodeName + ".";
        return false;
    }
    if (modulo.getNodes().size() >= 40)
    {
        error = "This demo stops at 40 servers.";
        return false;
    }

    int nodesBefore = (int)modulo.getNodes().size();

    // 1. remember the current owners
    std::vector<std::string> moduloBefore = moduloOwners;
    std::vector<std::string> ringBefore = ringOwners;
    std::vector<std::string> boundedBefore = boundedOwners;

    // 2. change the cluster
    modulo.addNode(nodeName);
    ring.addNode(nodeName);
    assignColor(nodeName);

    // 3. find the new owners
    computeAllOwners();

    // 4. compare
    result.label = "+" + nodeName;
    result.description = "Added " + nodeName;
    result.nodesBefore = nodesBefore;
    result.nodesAfter = nodesBefore + 1;
    result.modulo = compareOwners(moduloBefore, moduloOwners, records, EXAMPLES_TO_KEEP);
    result.consistent = compareOwners(ringBefore, ringOwners, records, EXAMPLES_TO_KEEP);
    result.bounded = compareOwners(boundedBefore, boundedOwners, records, EXAMPLES_TO_KEEP);

    // A perfectly balanced cluster would hand the new server 1/(N+1) of keys
    result.idealKeysMoved = (int)records.size() / result.nodesAfter;
    result.idealPercent = 100.0 / result.nodesAfter;

    return true;
}

bool ShardedCluster::removeNode(const std::string& nodeName, OperationResult& result, std::string& error)
{
    if (!hasNode(nodeName))
    {
        error = "There is no server called " + nodeName + ".";
        return false;
    }
    if (modulo.getNodes().size() <= 1)
    {
        error = "Can't remove the last server, the data has to live somewhere.";
        return false;
    }

    int nodesBefore = (int)modulo.getNodes().size();

    // 1. remember the current owners
    std::vector<std::string> moduloBefore = moduloOwners;
    std::vector<std::string> ringBefore = ringOwners;
    std::vector<std::string> boundedBefore = boundedOwners;

    // 2. change the cluster
    modulo.removeNode(nodeName);
    ring.removeNode(nodeName);

    // 3. find the new owners
    computeAllOwners();

    // 4. compare
    result.label = "-" + nodeName;
    result.description = "Removed " + nodeName;
    result.nodesBefore = nodesBefore;
    result.nodesAfter = nodesBefore - 1;
    result.modulo = compareOwners(moduloBefore, moduloOwners, records, EXAMPLES_TO_KEEP);
    result.consistent = compareOwners(ringBefore, ringOwners, records, EXAMPLES_TO_KEEP);
    result.bounded = compareOwners(boundedBefore, boundedOwners, records, EXAMPLES_TO_KEEP);

    // The removed server held about 1/N of the keys; those have to move
    result.idealKeysMoved = (int)records.size() / nodesBefore;
    result.idealPercent = 100.0 / nodesBefore;

    return true;
}

void ShardedCluster::computeModuloOwners(std::vector<std::string>& owners) const
{
    owners.resize(records.size());
    for (size_t i = 0; i < records.size(); i++)
    {
        owners[i] = modulo.getNodeForKey(records[i].key);
    }
}

void ShardedCluster::computeRingOwners(std::vector<std::string>& owners) const
{
    owners.resize(records.size());
    for (size_t i = 0; i < records.size(); i++)
    {
        owners[i] = ring.getNodeForKey(records[i].key);
    }
}

void ShardedCluster::computeAllOwners()
{
    computeModuloOwners(moduloOwners);
    computeRingOwners(ringOwners);

    // Strategy 3 uses the same ring, but places all keys in one pass
    // because of the capacity limit (see bounded_hashing.cpp)
    assignWithLoadLimit(ring, records, boundedOwners);
}

void ShardedCluster::assignColor(const std::string& nodeName)
{
    colorIndexOfNode[nodeName] = nextColorIndex % 12;
    nextColorIndex++;
}

std::string ShardedCluster::getNodeColor(const std::string& nodeName) const
{
    std::map<std::string, int>::const_iterator it = colorIndexOfNode.find(nodeName);
    if (it == colorIndexOfNode.end())
    {
        return "#999999";
    }
    return NODE_COLORS[it->second];
}

std::map<std::string, std::string> ShardedCluster::getAllNodeColors() const
{
    std::map<std::string, std::string> colors;
    const std::vector<std::string>& nodes = modulo.getNodes();
    for (size_t i = 0; i < nodes.size(); i++)
    {
        colors[nodes[i]] = getNodeColor(nodes[i]);
    }
    return colors;
}

const std::vector<std::string>& ShardedCluster::getNodes() const { return modulo.getNodes(); }
const std::vector<DataRecord>& ShardedCluster::getRecords() const { return records; }
const std::vector<std::string>& ShardedCluster::getModuloOwners() const { return moduloOwners; }
const std::vector<std::string>& ShardedCluster::getRingOwners() const { return ringOwners; }
const std::vector<std::string>& ShardedCluster::getBoundedOwners() const { return boundedOwners; }
const ConsistentHashRing& ShardedCluster::getRing() const { return ring; }
int ShardedCluster::getVnodesPerNode() const { return ring.getVirtualNodesPerNode(); }
unsigned int ShardedCluster::getSeed() const { return seed; }
