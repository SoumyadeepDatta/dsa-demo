// hashing.h
//
// The one hash function used by the whole project.
//
// Both sharding strategies (modulo and consistent hashing) use exactly the
// same hash function. That is on purpose: when they are compared, the only
// difference should be HOW the hash number is turned into a server, not the
// hash itself.

#ifndef HASHING_H
#define HASHING_H

#include <string>

// Turns any text (a key like "user:42", or a virtual node label like
// "node-3#vn7") into a 32 bit number between 0 and 4,294,967,295.
//
// The same text always gives the same number, and texts that are only a
// little different give very different numbers.
unsigned int hashString(const std::string& text);

#endif
