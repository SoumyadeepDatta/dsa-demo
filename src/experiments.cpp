// experiments.cpp
//
// Both experiments reuse ShardedCluster, so they measure exactly the same
// thing as the manual page. The only difference is that the program itself
// picks what to add and remove.

#include "experiments.h"
#include "text_utils.h"

#include <cstdlib>

SequenceResult runRandomSequence(const SequenceSettings& settings)
{
    SequenceResult result;
    result.totalModuloKeys = 0;
    result.totalConsistentKeys = 0;
    result.totalBoundedKeys = 0;
    result.totalModuloKB = 0;
    result.totalConsistentKB = 0;
    result.totalBoundedKB = 0;

    ShardedCluster cluster;
    cluster.setup(settings.keyCount, settings.vnodesPerNode, settings.startNodes, settings.seed);

    // Seed the random numbers again (setup() used them for the dataset) so
    // the choice of add/remove is also repeatable for a given seed.
    std::srand(settings.seed * 31 + 7);

    for (int step = 0; step < settings.steps; step++)
    {
        int nodeCount = (int)cluster.getNodes().size();

        // ---- decide: add or remove? ----
        bool doAdd;
        if (settings.mix == "grow")
        {
            doAdd = true;
        }
        else if (settings.mix == "shrink")
        {
            doAdd = false;
        }
        else
        {
            doAdd = (std::rand() % 2 == 0);
        }

        // keep the cluster between 2 and 40 servers in random mode, and stop
        // the other modes when they hit a limit
        if (nodeCount <= 2 && !doAdd)
        {
            if (settings.mix == "shrink") break;
            doAdd = true;
        }
        if (nodeCount >= 40 && doAdd)
        {
            if (settings.mix == "grow") break;
            doAdd = false;
        }

        // ---- do it ----
        OperationResult operation;
        std::string error;
        bool ok;

        if (doAdd)
        {
            ok = cluster.addNode(cluster.makeNextNodeName(), operation, error);
        }
        else
        {
            // pick any current server at random
            const std::vector<std::string>& nodes = cluster.getNodes();
            std::string victim = nodes[std::rand() % nodes.size()];
            ok = cluster.removeNode(victim, operation, error);
        }

        if (!ok)
        {
            break;
        }

        result.totalModuloKeys += operation.modulo.keysMoved;
        result.totalConsistentKeys += operation.consistent.keysMoved;
        result.totalBoundedKeys += operation.bounded.keysMoved;
        result.totalModuloKB += operation.modulo.dataMovedKB;
        result.totalConsistentKB += operation.consistent.dataMovedKB;
        result.totalBoundedKB += operation.bounded.dataMovedKB;
        result.steps.push_back(operation);
    }

    return result;
}

// How many different ring layouts are averaged in the virtual node test.
static const int TRIALS = 5;

// Builds a cluster for one trial of the virtual node test. The servers are
// called "t0-1", "t0-2", ... in trial 0, "t1-1", "t1-2", ... in trial 1 and
// so on. Virtual node positions come from hashing the server name, so new
// names give a completely new random layout of the ring.
static void buildTrialCluster(ShardedCluster& cluster, int keyCount, int vnodes,
                              int nodeCount, unsigned int seed, int trial)
{
    cluster.setup(keyCount, vnodes, 0, seed);

    for (int n = 1; n <= nodeCount; n++)
    {
        OperationResult ignored;
        std::string error;
        cluster.addNode("t" + intToText(trial) + "-" + intToText(n), ignored, error);
    }
}

VnodeTestResult runVnodeTest(int keyCount, int nodeCount, unsigned int seed)
{
    VnodeTestResult result;
    result.moduloBusiestExtraPercent = 0;

    const int vnodeCounts[9] = { 1, 2, 5, 10, 20, 50, 100, 200, 500 };

    for (int i = 0; i < 9; i++)
    {
        VnodeTestRow row;
        row.vnodes = vnodeCounts[i];
        row.busiestExtraPercent = 0;
        row.boundedExtraPercent = 0;

        // One ring layout can be lucky or unlucky, so the test is repeated
        // with TRIALS different layouts and the average is reported.
        for (int trial = 0; trial < TRIALS; trial++)
        {
            ShardedCluster cluster;
            buildTrialCluster(cluster, keyCount, vnodeCounts[i], nodeCount, seed + trial, trial);

            LoadSummary ringLoad = measureLoad(cluster.getRingOwners(), cluster.getNodes(), cluster.getRecords());
            row.busiestExtraPercent += ringLoad.busiestExtraPercent / TRIALS;

            LoadSummary boundedLoad = measureLoad(cluster.getBoundedOwners(), cluster.getNodes(), cluster.getRecords());
            row.boundedExtraPercent += boundedLoad.busiestExtraPercent / TRIALS;

            // Normal hashing does not depend on virtual nodes, so it is only
            // measured during the first round
            if (i == 0)
            {
                LoadSummary moduloLoad = measureLoad(cluster.getModuloOwners(), cluster.getNodes(), cluster.getRecords());
                result.moduloBusiestExtraPercent += moduloLoad.busiestExtraPercent / TRIALS;
            }
        }

        result.rows.push_back(row);
    }

    return result;
}
