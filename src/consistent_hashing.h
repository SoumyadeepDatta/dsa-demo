// consistent_hashing.h
//
// STRATEGY 2: consistent hashing with virtual nodes (vnodes).
//
// Picture every possible hash value (0 .. 4,294,967,295) laid out on a
// circle, the "ring". Then:
//
//   * Each server is hashed onto the ring at several spots. Each spot is a
//     "virtual node". A server with 100 vnodes sits at 100 places.
//   * A key is hashed onto the same ring.
//   * Starting from the key, walk clockwise. The first virtual node found
//     on the way is the server that owns the key.
//
// When a server is added, only the keys between its new vnodes and the
// vnodes just before them change owner. When a server is removed, only the
// keys it owned move (to the next vnode clockwise). Everybody else stays put.
//
// Why vnodes? With only one spot per server the gaps between servers are
// random and very uneven, so one server might own 40% of the ring and another
// 5%. Many small spots per server average out those gaps.
//
// Data structure: a std::map from ring position -> server name. std::map
// keeps its entries sorted, so "first vnode clockwise from position X" is just
// map.lower_bound(X), which is a binary search (O(log V)).

#ifndef CONSISTENT_HASHING_H
#define CONSISTENT_HASHING_H

#include <string>
#include <vector>
#include <map>

class ConsistentHashRing
{
public:
    ConsistentHashRing();

    // How many virtual nodes each server gets. Changing this rebuilds the
    // ring for all existing servers.
    void setVirtualNodesPerNode(int count);
    int getVirtualNodesPerNode() const;

    // Places a server's virtual nodes on the ring.
    void addNode(const std::string& nodeName);

    // Takes a server's virtual nodes off the ring.
    // Returns false if no server has that name.
    bool removeNode(const std::string& nodeName);

    // Finds the server for a key by walking clockwise on the ring.
    std::string getNodeForKey(const std::string& key) const;

    // The real servers (not the virtual ones).
    const std::vector<std::string>& getNodes() const;

    // Every virtual node, sorted by position. Used to draw the ring.
    const std::map<unsigned int, std::string>& getRingPoints() const;

    // For each server: what percentage of the ring it is responsible for.
    // In a perfect world every server would own 100 / N percent.
    std::map<std::string, double> getRingOwnershipPercent() const;

    // Removes every server.
    void clear();

    // The label that is hashed to get the position of a virtual node.
    // Example: makeVirtualNodeLabel("node-2", 5) -> "node-2#vn5"
    static std::string makeVirtualNodeLabel(const std::string& nodeName, int vnodeNumber);

private:
    // Puts all vnodes of one server onto the ring.
    void placeVirtualNodes(const std::string& nodeName);

    // position on ring -> real server name
    std::map<unsigned int, std::string> ring;

    // real servers in the order they were added
    std::vector<std::string> nodeNames;

    int virtualNodesPerNode;
};

#endif
