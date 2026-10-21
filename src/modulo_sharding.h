// modulo_sharding.h
//
// STRATEGY 1: "normal" hashing, also called hash-mod-N sharding.
//
//     server index = hash(key) % number_of_servers
//
// It is the first thing most people write, and it spreads keys evenly.
// The catch: the answer depends on number_of_servers. As soon as a server is
// added or removed, N changes and almost every key gets a new index, so almost
// all the data has to be copied to a different machine.

#ifndef MODULO_SHARDING_H
#define MODULO_SHARDING_H

#include <string>
#include <vector>

class ModuloSharding
{
public:
    // Adds a server to the END of the server list.
    void addNode(const std::string& nodeName);

    // Removes a server from the list. Every server after it moves one
    // position to the left, which also changes their index.
    // Returns false if no server has that name.
    bool removeNode(const std::string& nodeName);

    // Decides which server stores the given key.
    std::string getNodeForKey(const std::string& key) const;

    // The servers in their current order (index 0, 1, 2, ...).
    const std::vector<std::string>& getNodes() const;

    // Removes every server.
    void clear();

private:
    // Position in this list = the server index used by the formula.
    std::vector<std::string> nodeNames;
};

#endif
