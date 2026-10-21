// bounded_hashing.cpp
//
// Implementation of consistent hashing with a load limit.
// Compare with ConsistentHashRing::getNodeForKey(): the only new part is the
// while loop that skips servers that are already full.

#include "bounded_hashing.h"
#include "hashing.h"

#include <map>
#include <cmath>

void assignWithLoadLimit(const ConsistentHashRing& ring,
                         const std::vector<DataRecord>& records,
                         std::vector<std::string>& owners)
{
    owners.assign(records.size(), "");

    const std::map<unsigned int, std::string>& points = ring.getRingPoints();
    int serverCount = (int)ring.getNodes().size();

    if (points.empty() || serverCount == 0)
    {
        return;
    }

    // ---- Step 1: capacity of every server ----
    // Example: 10,000 keys, 4 servers -> average 2500 -> capacity 2750.
    // ceil() rounds up, so all capacities together are always more than the
    // number of keys, which means every key is guaranteed to find a place.
    double average = (double)records.size() / serverCount;
    int capacity = (int)std::ceil(average * (1.0 + LOAD_LIMIT));

    // ---- Step 2: a counter for how many keys each server has so far ----
    std::map<std::string, int> load;
    for (int i = 0; i < serverCount; i++)
    {
        load[ring.getNodes()[i]] = 0;
    }

    // ---- Step 3: place the keys one by one ----
    for (size_t i = 0; i < records.size(); i++)
    {
        // Same start as normal consistent hashing: first point clockwise
        unsigned int keyPosition = hashString(records[i].key);
        std::map<unsigned int, std::string>::const_iterator spot = points.lower_bound(keyPosition);
        if (spot == points.end())
        {
            spot = points.begin();
        }

        // Linear probing: while this server is full, try the next point
        // clockwise (wrapping around at the end of the ring).
        while (load[spot->second] >= capacity)
        {
            ++spot;
            if (spot == points.end())
            {
                spot = points.begin();
            }
        }

        owners[i] = spot->second;
        load[spot->second]++;
    }
}
