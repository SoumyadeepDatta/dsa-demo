// modulo_sharding.cpp
//
// Implementation of hash % N sharding. The whole algorithm is really the
// three lines inside getNodeForKey(); the rest is list bookkeeping.

#include "modulo_sharding.h"
#include "hashing.h"

void ModuloSharding::addNode(const std::string& nodeName)
{
    nodeNames.push_back(nodeName);
}

bool ModuloSharding::removeNode(const std::string& nodeName)
{
    for (size_t i = 0; i < nodeNames.size(); i++)
    {
        if (nodeNames[i] == nodeName)
        {
            nodeNames.erase(nodeNames.begin() + i);
            return true;
        }
    }

    return false;
}

std::string ModuloSharding::getNodeForKey(const std::string& key) const
{
    if (nodeNames.empty())
    {
        return "";
    }

    // 1. hash the key into a big number
    unsigned int hashValue = hashString(key);

    // 2. squeeze that number into the range 0 .. N-1
    unsigned int serverIndex = hashValue % (unsigned int)nodeNames.size();

    // 3. that index is the server
    return nodeNames[serverIndex];
}

const std::vector<std::string>& ModuloSharding::getNodes() const
{
    return nodeNames;
}

void ModuloSharding::clear()
{
    nodeNames.clear();
}
