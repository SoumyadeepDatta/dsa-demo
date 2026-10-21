// hashing.cpp
//
// This uses FNV-1a, which is one of the simplest well known hash functions:
// start with a fixed number, and for every character "mix in" that
// character with an XOR and then a multiply.
//
// FNV-1a on its own is a bit weak when two inputs differ only in their last
// character (like "node-1#vn1" and "node-1#vn2"), the results can end up
// close together. On a hash ring that would make virtual nodes clump. So at
// the end the result goes through a short "finalizer" (taken from
// MurmurHash3) that scrambles the bits properly.

#include "hashing.h"

unsigned int hashString(const std::string& text)
{
    // ---- Step 1: FNV-1a ----
    // 2166136261 and 16777619 are the official FNV constants for 32 bits.
    unsigned int hash = 2166136261u;

    for (size_t i = 0; i < text.size(); i++)
    {
        unsigned char character = (unsigned char)text[i];

        hash = hash ^ character;     // mix the character into the low bits
        hash = hash * 16777619u;     // spread it out over all the bits
    }

    // ---- Step 2: finalizer ----
    // Shift-XOR-multiply a few times. After this, changing one input bit
    // changes about half of the output bits.
    hash = hash ^ (hash >> 16);
    hash = hash * 0x85ebca6bu;
    hash = hash ^ (hash >> 13);
    hash = hash * 0xc2b2ae35u;
    hash = hash ^ (hash >> 16);

    return hash;
}
