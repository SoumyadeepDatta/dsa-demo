// bounded_hashing.h
//
// STRATEGY 3: consistent hashing with a load limit ("bounded loads").
//
// This is the same hash ring as in consistent_hashing.h, with one change
// taken from hash tables:
//
//   * Every server gets a capacity:
//         capacity = average keys per server * (1 + LOAD_LIMIT)
//     With LOAD_LIMIT = 0.10 no server may hold more than 10% above average.
//
//   * A key starts at its normal spot on the ring and walks clockwise, like
//     before. If the server it reaches is already full, it simply keeps
//     walking to the next server point that still has room.
//
// The "keep walking to the next free slot" idea is LINEAR PROBING, the
// collision handling used in open addressing hash tables. Here the "slots"
// are the server points on the ring, and "occupied" means "server is full".
//
// DSA used:
//   - ordered map (std::map, a balanced binary search tree) for the ring,
//     lower_bound() finds the starting point in O(log V)
//   - linear probing to skip full servers
//   - hash map / counter (std::map server -> key count) to know how full
//     each server is
//
// Because a key's server depends on how full the servers already are, keys
// can't be placed one at a time independently. All keys are placed in one
// pass, always in the same order, so the result is repeatable.

#ifndef BOUNDED_HASHING_H
#define BOUNDED_HASHING_H

#include <string>
#include <vector>

#include "consistent_hashing.h"
#include "dataset.h"

// How far above average a server may go. 0.10 = 10%.
const double LOAD_LIMIT = 0.10;

// Fills owners[i] with the server for records[i], using the ring from
// consistent hashing plus the capacity rule described above.
void assignWithLoadLimit(const ConsistentHashRing& ring,
                         const std::vector<DataRecord>& records,
                         std::vector<std::string>& owners);

#endif
