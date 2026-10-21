// consistent_hashing.cpp
//
// Implementation of the hash ring described in consistent_hashing.h.
// The important function is getNodeForKey(), it is short on purpose.

#include "consistent_hashing.h"
#include "hashing.h"
#include "text_utils.h"

ConsistentHashRing::ConsistentHashRing()
{
    virtualNodesPerNode = 1;
}

std::string ConsistentHashRing::makeVirtualNodeLabel(const std::string& nodeName, int vnodeNumber)
{
    return nodeName + "#vn" + intToText(vnodeNumber);
}

void ConsistentHashRing::setVirtualNodesPerNode(int count)
{
    if (count < 1)
    {
        count = 1;
    }
    virtualNodesPerNode = count;

    // Every server needs a new set of vnodes, so throw the ring away and
    // place all servers again.
    ring.clear();
    for (size_t i = 0; i < nodeNames.size(); i++)
    {
        placeVirtualNodes(nodeNames[i]);
    }
}

int ConsistentHashRing::getVirtualNodesPerNode() const
{
    return virtualNodesPerNode;
}

void ConsistentHashRing::placeVirtualNodes(const std::string& nodeName)
{
    for (int i = 0; i < virtualNodesPerNode; i++)
    {
        std::string label = makeVirtualNodeLabel(nodeName, i);
        unsigned int position = hashString(label);

        // Two labels can, very rarely, hash to the same position. In that
        // case the spot is already taken and this vnode is skipped.
        // With 4 billion positions this basically never happens.
        if (ring.find(position) == ring.end())
        {
            ring[position] = nodeName;
        }
    }
}

void ConsistentHashRing::addNode(const std::string& nodeName)
{
    nodeNames.push_back(nodeName);
    placeVirtualNodes(nodeName);
}

bool ConsistentHashRing::removeNode(const std::string& nodeName)
{
    // First take the server out of the list of real servers.
    bool found = false;
    for (size_t i = 0; i < nodeNames.size(); i++)
    {
        if (nodeNames[i] == nodeName)
        {
            nodeNames.erase(nodeNames.begin() + i);
            found = true;
            break;
        }
    }

    if (!found)
    {
        return false;
    }

    // Then remove its vnodes. There is no need to search the whole ring: the
    // positions come from the hash of "name#vnX", so they can simply be
    // computed again and exactly those spots erased.
    for (int i = 0; i < virtualNodesPerNode; i++)
    {
        unsigned int position = hashString(makeVirtualNodeLabel(nodeName, i));

        std::map<unsigned int, std::string>::iterator spot = ring.find(position);
        if (spot != ring.end() && spot->second == nodeName)
        {
            ring.erase(spot);
        }
    }

    return true;
}

std::string ConsistentHashRing::getNodeForKey(const std::string& key) const
{
    if (ring.empty())
    {
        return "";
    }

    // 1. put the key on the ring
    unsigned int keyPosition = hashString(key);

    // 2. find the first vnode at or after that position (clockwise)
    std::map<unsigned int, std::string>::const_iterator owner = ring.lower_bound(keyPosition);

    // 3. if the search went past the largest position, wrap around to the start
    //    of the ring. That's what makes it a circle and not a line.
    if (owner == ring.end())
    {
        owner = ring.begin();
    }

    return owner->second;
}

const std::vector<std::string>& ConsistentHashRing::getNodes() const
{
    return nodeNames;
}

const std::map<unsigned int, std::string>& ConsistentHashRing::getRingPoints() const
{
    return ring;
}

std::map<std::string, double> ConsistentHashRing::getRingOwnershipPercent() const
{
    std::map<std::string, double> share;

    for (size_t i = 0; i < nodeNames.size(); i++)
    {
        share[nodeNames[i]] = 0.0;
    }

    if (ring.empty())
    {
        return share;
    }

    // A single vnode owns the whole circle.
    if (ring.size() == 1)
    {
        share[ring.begin()->second] = 100.0;
        return share;
    }

    // Each vnode owns the arc that ends at it, starting right after the
    // previous vnode. For the very first vnode the "previous" one is the last
    // vnode on the ring (wrap around).
    //
    // Nice detail: with unsigned numbers, (small - big) wraps around too, so
    // the first vnode's arc length comes out right without any special case.
    const double fullRing = 4294967296.0;   // 2^32 positions
    unsigned int previousPosition = ring.rbegin()->first;

    std::map<unsigned int, std::string>::const_iterator it;
    for (it = ring.begin(); it != ring.end(); ++it)
    {
        unsigned int arcLength = it->first - previousPosition;
        share[it->second] += arcLength / fullRing * 100.0;
        previousPosition = it->first;
    }

    return share;
}

void ConsistentHashRing::clear()
{
    ring.clear();
    nodeNames.clear();
}
