// dataset.cpp
//
// Creates a list of records that looks a bit like a real app's data: a mix
// of users, orders, carts and sessions, with sizes that are mostly small and
// sometimes large.

#include "dataset.h"
#include "text_utils.h"

#include <cstdlib>

std::vector<DataRecord> generateDataset(int count, unsigned int seed)
{
    // srand + rand are the simplest random numbers in C++. Setting the seed
    // first makes the sequence repeatable.
    std::srand(seed);

    const char* prefixes[4] = { "user:", "order:", "cart:", "session:" };

    std::vector<DataRecord> records;
    records.reserve(count);

    for (int i = 0; i < count; i++)
    {
        DataRecord record;

        // Pick a table name at random and give the row a unique number.
        int table = std::rand() % 4;
        record.key = std::string(prefixes[table]) + intToText(i + 1);

        // Size: most rows are 1-16 KB, about one in ten is 16-256 KB.
        if (std::rand() % 10 == 0)
        {
            record.sizeKB = 16 + std::rand() % 241;
        }
        else
        {
            record.sizeKB = 1 + std::rand() % 16;
        }

        records.push_back(record);
    }

    return records;
}

long long totalDataKB(const std::vector<DataRecord>& records)
{
    long long total = 0;
    for (size_t i = 0; i < records.size(); i++)
    {
        total += records[i].sizeKB;
    }
    return total;
}
