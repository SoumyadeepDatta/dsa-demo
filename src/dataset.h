// dataset.h
//
// The fake "database rows" that get sharded. Every record has a key (what
// gets hashed) and a size (so the program can show how much DATA moves, not
// only how many keys move - a few big rows can cost more than many tiny ones).

#ifndef DATASET_H
#define DATASET_H

#include <string>
#include <vector>

struct DataRecord
{
    std::string key;   // e.g. "user:1042" or "order:77"
    int sizeKB;        // how big the row is, in kilobytes
};

// Builds `count` records. The same seed always gives the same records, so
// experiments can be repeated and compared.
std::vector<DataRecord> generateDataset(int count, unsigned int seed);

// Adds up the size of all records.
long long totalDataKB(const std::vector<DataRecord>& records);

#endif
