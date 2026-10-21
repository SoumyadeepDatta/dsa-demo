// experiments.h
//
// Automatic experiments that run without anyone clicking buttons.
//
//   1. Random sequence: start with some servers, then add/remove servers at
//      random for a number of steps and record what moved each time.
//
//   2. Virtual node test: build the same cluster with 1, 2, 5, 10 ... 500
//      virtual nodes per server and check how evenly the keys are spread.
//      This shows WHY virtual nodes are used.

#ifndef EXPERIMENTS_H
#define EXPERIMENTS_H

#include <string>
#include <vector>

#include "cluster.h"

// ---- Experiment 1 ----

struct SequenceSettings
{
    int keyCount;
    int startNodes;
    int steps;
    int vnodesPerNode;
    std::string mix;     // "random", "grow" (only add) or "shrink" (only remove)
    unsigned int seed;
};

struct SequenceResult
{
    std::vector<OperationResult> steps;

    int totalModuloKeys;          // keys moved over all steps
    int totalConsistentKeys;
    int totalBoundedKeys;
    long long totalModuloKB;      // data moved over all steps
    long long totalConsistentKB;
    long long totalBoundedKB;
};

SequenceResult runRandomSequence(const SequenceSettings& settings);

// ---- Experiment 2 ----

struct VnodeTestRow
{
    int vnodes;                   // virtual nodes per server
    double busiestExtraPercent;   // busiest server: % more keys than average
    double boundedExtraPercent;   // same, for consistent hashing with load limit
};

struct VnodeTestResult
{
    std::vector<VnodeTestRow> rows;   // consistent hashing, one row per vnode count

    // Normal hashing does not use virtual nodes, so it has a single value
    // that is drawn as a flat line for comparison.
    double moduloBusiestExtraPercent;
};

VnodeTestResult runVnodeTest(int keyCount, int nodeCount, unsigned int seed);

#endif
