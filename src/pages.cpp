// pages.cpp
//
// Builds the HTML for every page. HTML attributes use single quotes ('...')
// so double quotes inside the C++ strings don't need escaping.
//
// The pages contain no JavaScript. Buttons are plain HTML forms that send a
// request to the C++ server, the server updates the cluster, and sends back
// a freshly built page with new tables and SVG charts.
//
// Wording on the page:
//   "Normal hashing"                  = hash(key) % N
//   "Consistent hashing"              = hash ring with virtual nodes
//   "Consistent hashing + load limit" = same ring, but full servers are
//                                       skipped (linear probing)

#include "pages.h"
#include "svg_charts.h"
#include "text_utils.h"
#include "experiments.h"
#include "bounded_hashing.h"

#include <sstream>

// Fixed colours for the two strategies, used in every chart.
static const std::string NORMAL_COLOR = "#cc4422";
static const std::string CONSISTENT_COLOR = "#2266aa";
static const std::string BOUNDED_COLOR = "#228833";

static const std::string NORMAL_NAME = "Normal hashing";
static const std::string CONSISTENT_NAME = "Consistent hashing";
static const std::string BOUNDED_NAME = "Consistent hashing + load limit";

// Makes a chart series with no values yet.
static ChartSeries makeSeries(const std::string& name, const std::string& color)
{
    ChartSeries series;
    series.name = name;
    series.color = color;
    return series;
}

// Puts the three series of a chart into one list, in the usual order.
static std::vector<ChartSeries> threeSeries(const ChartSeries& normal, const ChartSeries& consistent,
                                            const ChartSeries& bounded)
{
    std::vector<ChartSeries> list;
    list.push_back(normal);
    list.push_back(consistent);
    list.push_back(bounded);
    return list;
}

// Table header row with the three method names.
static std::string methodHeaderRow()
{
    return "<tr><th></th><th>" + NORMAL_NAME + "</th><th>" + CONSISTENT_NAME + "</th><th>" + BOUNDED_NAME + "</th></tr>\n";
}

// ---------------------------------------------------------------------------
// Shared page frame
// ---------------------------------------------------------------------------

// Only the bare minimum: borders for tables and a size for the charts.
static const char* PAGE_STYLE = R"CSS(
body { font-family: Arial, sans-serif; margin: 16px; max-width: 1000px; background: #fff; color: #000; }
table { border-collapse: collapse; margin: 6px 0 10px 0; }
th, td { border: 1px solid #999; padding: 3px 8px; text-align: left; }
svg.chart { width: 100%; max-width: 680px; height: auto; display: block; margin: 8px 0; }
svg.ring { width: 100%; max-width: 380px; height: auto; display: block; }
input[type=number] { width: 80px; }
)CSS";

static std::string pageStart(const std::string& title)
{
    std::ostringstream html;
    html << "<!doctype html>\n<html lang='en'>\n<head>\n<meta charset='utf-8'>\n"
         << "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
         << "<meta name='color-scheme' content='light'>\n"
         << "<title>" << escapeHtml(title) << "</title>\n"
         << "<style>" << PAGE_STYLE << "</style>\n</head>\n<body>\n";

    html << "<h1>Normal Hashing vs Consistent Hashing vs Consistent Hashing with Load Limit</h1>\n"
         << "<p><a href='/'>Manual test</a> | <a href='/experiments'>Random test</a> | "
         << "<a href='/notes'>Explanation</a></p>\n<hr>\n";

    return html.str();
}

static std::string pageEnd()
{
    return "</body>\n</html>\n";
}

// A small coloured square, used next to server names so they can be
// matched with the colours on the hash ring.
static std::string colorMark(const std::string& color)
{
    return "<span style='color:" + color + "'>&#9632;</span> ";
}

// Example: "8039 (80.4%)"
static std::string countWithPercent(int count, double percent)
{
    return intToText(count) + " (" + decimalToText(percent, 1) + "%)";
}

// ---------------------------------------------------------------------------
// Manual test page ("/")
// ---------------------------------------------------------------------------

// List of servers with a remove button each, plus the "add server" form.
static std::string buildServerSection(const ShardedCluster& cluster)
{
    std::ostringstream html;

    const std::vector<std::string>& nodes = cluster.getNodes();
    LoadSummary normalLoad = measureLoad(cluster.getModuloOwners(), nodes, cluster.getRecords());
    LoadSummary consistentLoad = measureLoad(cluster.getRingOwners(), nodes, cluster.getRecords());
    LoadSummary boundedLoad = measureLoad(cluster.getBoundedOwners(), nodes, cluster.getRecords());

    html << "<h2>Servers</h2>\n";
    html << "<p>Total keys: " << cluster.getRecords().size()
         << " &nbsp; Total data: " << formatDataSize(totalDataKB(cluster.getRecords()))
         << " &nbsp; Virtual nodes per server: " << cluster.getVnodesPerNode() << "</p>\n";

    html << "<table><tr><th>Server</th><th>Keys (normal)</th><th>Keys (consistent)</th><th>Keys (load limit)</th><th></th></tr>\n";
    for (size_t i = 0; i < nodes.size(); i++)
    {
        html << "<tr><td>" << colorMark(cluster.getNodeColor(nodes[i])) << escapeHtml(nodes[i]) << "</td>"
             << "<td>" << normalLoad.perNode[i].keyCount << "</td>"
             << "<td>" << consistentLoad.perNode[i].keyCount << "</td>"
             << "<td>" << boundedLoad.perNode[i].keyCount << "</td>"
             << "<td><form method='post' action='/remove' style='margin:0'>"
             << "<input type='hidden' name='name' value='" << escapeHtml(nodes[i]) << "'>"
             << "<button type='submit'>Remove</button></form></td></tr>\n";
    }
    html << "</table>\n";

    html << "<form method='post' action='/add'>"
         << "New server name: <input type='text' name='name' maxlength='20' placeholder='optional'> "
         << "<button type='submit'>Add server</button></form>\n";

    return html.str();
}

// A few example keys that moved, written as a simple list.
static std::string movedKeyList(const std::string& heading, const std::vector<MovedKey>& examples)
{
    std::ostringstream html;
    html << "<p>" << heading << ": ";

    if (examples.empty())
    {
        html << "no keys moved.</p>\n";
        return html.str();
    }

    size_t shown = examples.size() < 5 ? examples.size() : 5;
    for (size_t i = 0; i < shown; i++)
    {
        if (i > 0) html << ", ";
        html << escapeHtml(examples[i].key) << " (" << escapeHtml(examples[i].fromNode)
             << " &rarr; " << escapeHtml(examples[i].toNode) << ")";
    }
    html << " ...</p>\n";
    return html.str();
}

// Result of the most recent add/remove.
static std::string buildLastChange(const OperationResult& op)
{
    std::ostringstream html;

    html << "<h2>Last change: " << escapeHtml(op.description)
         << " (" << op.nodesBefore << " &rarr; " << op.nodesAfter << " servers)</h2>\n";

    html << "<table>" << methodHeaderRow();
    html << "<tr><td>Keys moved</td>"
         << "<td>" << countWithPercent(op.modulo.keysMoved, op.modulo.percentKeysMoved) << "</td>"
         << "<td>" << countWithPercent(op.consistent.keysMoved, op.consistent.percentKeysMoved) << "</td>"
         << "<td>" << countWithPercent(op.bounded.keysMoved, op.bounded.percentKeysMoved) << "</td></tr>\n";
    html << "<tr><td>Data moved</td>"
         << "<td>" << formatDataSize(op.modulo.dataMovedKB) << "</td>"
         << "<td>" << formatDataSize(op.consistent.dataMovedKB) << "</td>"
         << "<td>" << formatDataSize(op.bounded.dataMovedKB) << "</td></tr>\n";
    html << "</table>\n";

    html << "<p>Smallest possible movement for this change: about " << op.idealKeysMoved
         << " keys (" << decimalToText(op.idealPercent, 1) << "%).</p>\n";

    html << movedKeyList("Some keys moved by normal hashing", op.modulo.examples);
    html << movedKeyList("Some keys moved by consistent hashing", op.consistent.examples);
    html << movedKeyList("Some keys moved by consistent hashing + load limit", op.bounded.examples);

    return html.str();
}

// Charts over all changes made so far.
static std::string buildHistory(const std::vector<OperationResult>& history)
{
    std::ostringstream html;
    html << "<h2>All changes so far</h2>\n";

    if (history.empty())
    {
        html << "<p>No changes yet. Add or remove a server above to see the results.</p>\n";
        return html.str();
    }

    std::vector<std::string> labels;
    ChartSeries normalPercent = makeSeries(NORMAL_NAME, NORMAL_COLOR);
    ChartSeries consistentPercent = makeSeries(CONSISTENT_NAME, CONSISTENT_COLOR);
    ChartSeries boundedPercent = makeSeries(BOUNDED_NAME, BOUNDED_COLOR);
    ChartSeries normalTotal = makeSeries(NORMAL_NAME, NORMAL_COLOR);
    ChartSeries consistentTotal = makeSeries(CONSISTENT_NAME, CONSISTENT_COLOR);
    ChartSeries boundedTotal = makeSeries(BOUNDED_NAME, BOUNDED_COLOR);

    double normalRunningMB = 0;
    double consistentRunningMB = 0;
    double boundedRunningMB = 0;

    for (size_t i = 0; i < history.size(); i++)
    {
        labels.push_back(history[i].label);
        normalPercent.values.push_back(history[i].modulo.percentKeysMoved);
        consistentPercent.values.push_back(history[i].consistent.percentKeysMoved);
        boundedPercent.values.push_back(history[i].bounded.percentKeysMoved);

        normalRunningMB += history[i].modulo.dataMovedKB / 1024.0;
        consistentRunningMB += history[i].consistent.dataMovedKB / 1024.0;
        boundedRunningMB += history[i].bounded.dataMovedKB / 1024.0;
        normalTotal.values.push_back(normalRunningMB);
        consistentTotal.values.push_back(consistentRunningMB);
        boundedTotal.values.push_back(boundedRunningMB);
    }

    html << drawBarChart("Keys moved in each change", labels,
                         threeSeries(normalPercent, consistentPercent, boundedPercent), "% of all keys");
    html << drawLineChart("Total data moved so far", labels,
                          threeSeries(normalTotal, consistentTotal, boundedTotal), "MB");

    return html.str();
}

// How evenly the keys are spread right now.
static std::string buildLoad(const ShardedCluster& cluster)
{
    std::ostringstream html;
    const std::vector<std::string>& nodes = cluster.getNodes();

    LoadSummary normalLoad = measureLoad(cluster.getModuloOwners(), nodes, cluster.getRecords());
    LoadSummary consistentLoad = measureLoad(cluster.getRingOwners(), nodes, cluster.getRecords());
    LoadSummary boundedLoad = measureLoad(cluster.getBoundedOwners(), nodes, cluster.getRecords());

    ChartSeries normalKeys = makeSeries(NORMAL_NAME, NORMAL_COLOR);
    ChartSeries consistentKeys = makeSeries(CONSISTENT_NAME, CONSISTENT_COLOR);
    ChartSeries boundedKeys = makeSeries(BOUNDED_NAME, BOUNDED_COLOR);

    for (size_t i = 0; i < nodes.size(); i++)
    {
        normalKeys.values.push_back(normalLoad.perNode[i].keyCount);
        consistentKeys.values.push_back(consistentLoad.perNode[i].keyCount);
        boundedKeys.values.push_back(boundedLoad.perNode[i].keyCount);
    }

    html << "<h2>Keys on each server</h2>\n";
    html << drawBarChart("Keys on each server", nodes,
                         threeSeries(normalKeys, consistentKeys, boundedKeys), "keys");

    html << "<p>Average per server: " << decimalToText(normalLoad.averageKeys, 0) << " keys.<br>\n"
         << "Normal hashing: busiest server has " << normalLoad.mostKeys << " keys ("
         << decimalToText(normalLoad.busiestExtraPercent, 1) << "% above average).<br>\n"
         << "Consistent hashing: busiest server has " << consistentLoad.mostKeys << " keys ("
         << decimalToText(consistentLoad.busiestExtraPercent, 1) << "% above average).<br>\n"
         << "Consistent hashing + load limit: busiest server has " << boundedLoad.mostKeys << " keys ("
         << decimalToText(boundedLoad.busiestExtraPercent, 1) << "% above average, the limit is "
         << decimalToText(LOAD_LIMIT * 100, 0) << "%).</p>\n";

    return html.str();
}

// The ring picture and how much of it each server owns.
static std::string buildRing(const ShardedCluster& cluster)
{
    std::ostringstream html;
    const ConsistentHashRing& ring = cluster.getRing();
    const std::vector<std::string>& nodes = cluster.getNodes();

    // The first 30 keys are drawn as dots on the ring
    std::vector<std::string> sampleKeys;
    for (size_t i = 0; i < cluster.getRecords().size() && i < 30; i++)
    {
        sampleKeys.push_back(cluster.getRecords()[i].key);
    }

    std::map<std::string, double> share = ring.getRingOwnershipPercent();

    html << "<h2>Hash ring (consistent hashing)</h2>\n"
         << "<p>Each colour is the part of the ring that belongs to one server. A key goes to the first "
         << "server point found when moving clockwise from the key. Black dots are some of the keys. "
         << "The load limit method uses this same ring, it only skips servers that are full.</p>\n";

    html << drawHashRing(ring, cluster.getAllNodeColors(), sampleKeys);

    html << "<table><tr><th>Server</th><th>Part of the ring</th></tr>\n";
    for (size_t i = 0; i < nodes.size(); i++)
    {
        html << "<tr><td>" << colorMark(cluster.getNodeColor(nodes[i])) << escapeHtml(nodes[i]) << "</td>"
             << "<td>" << decimalToText(share[nodes[i]], 1) << "%</td></tr>\n";
    }
    html << "</table>\n";

    return html.str();
}

// Form to rebuild the cluster with other settings.
static std::string buildResetForm(const ShardedCluster& cluster)
{
    std::ostringstream html;
    html << "<h2>Start again</h2>\n"
         << "<form method='post' action='/setup'>"
         << "Keys: <input type='number' name='keys' min='100' max='200000' value='" << cluster.getRecords().size() << "'> "
         << "Servers: <input type='number' name='nodes' min='1' max='40' value='" << cluster.getNodes().size() << "'> "
         << "Virtual nodes per server: <input type='number' name='vnodes' min='1' max='500' value='"
         << cluster.getVnodesPerNode() << "'> "
         << "Seed: <input type='number' name='seed' min='1' max='99999' value='" << cluster.getSeed() << "'> "
         << "<button type='submit'>Start again</button></form>\n"
         << "<p>Tip: try 1 virtual node per server, then 200, and compare the hash ring.</p>\n";
    return html.str();
}

std::string buildClusterPage(const ShardedCluster& cluster,
                             const std::vector<OperationResult>& history,
                             const std::string& message,
                             bool messageIsError)
{
    std::ostringstream html;
    html << pageStart("Manual test");

    if (!message.empty())
    {
        html << "<p style='color:" << (messageIsError ? "#b00" : "#070") << "'><b>"
             << escapeHtml(message) << "</b></p>\n";
    }

    html << buildServerSection(cluster);

    if (!history.empty())
    {
        html << "<hr>\n" << buildLastChange(history.back());
    }

    html << "<hr>\n" << buildHistory(history);
    html << "<hr>\n" << buildLoad(cluster);
    html << "<hr>\n" << buildRing(cluster);
    html << "<hr>\n" << buildResetForm(cluster);

    html << pageEnd();
    return html.str();
}

// ---------------------------------------------------------------------------
// Random test page ("/experiments")
// ---------------------------------------------------------------------------

// Helper for <select>: writes an <option>, marked selected if it matches.
static std::string option(const std::string& value, const std::string& text, const std::string& current)
{
    return "<option value='" + value + "'" + (value == current ? " selected" : "") + ">" + text + "</option>";
}

static std::string buildSequenceSection(const HttpRequest& request)
{
    // Read the form values (or defaults the first time the page is opened)
    SequenceSettings settings;
    settings.keyCount = textToInt(getParam(request, "keys", ""), 20000, 100, 200000);
    settings.startNodes = textToInt(getParam(request, "nodes", ""), 4, 1, 40);
    settings.steps = textToInt(getParam(request, "steps", ""), 15, 1, 60);
    settings.vnodesPerNode = textToInt(getParam(request, "vnodes", ""), 100, 1, 500);
    settings.seed = textToInt(getParam(request, "seed", ""), 7, 1, 99999);
    settings.mix = getParam(request, "mix", "random");
    if (settings.mix != "grow" && settings.mix != "shrink") settings.mix = "random";

    std::ostringstream html;
    html << "<h2>Test 1: random adding and removing of servers</h2>\n"
         << "<p>The program creates a cluster and then adds or removes servers at random. "
         << "All three methods get exactly the same changes.</p>\n"
         << "<form method='get' action='/experiments'><input type='hidden' name='run' value='sequence'>"
         << "Keys: <input type='number' name='keys' value='" << settings.keyCount << "'> "
         << "Starting servers: <input type='number' name='nodes' value='" << settings.startNodes << "'> "
         << "Steps: <input type='number' name='steps' value='" << settings.steps << "'> "
         << "Virtual nodes: <input type='number' name='vnodes' value='" << settings.vnodesPerNode << "'> "
         << "Seed: <input type='number' name='seed' value='" << settings.seed << "'> "
         << "Changes: <select name='mix'>"
         << option("random", "add and remove", settings.mix)
         << option("grow", "only add", settings.mix)
         << option("shrink", "only remove", settings.mix)
         << "</select> <button type='submit'>Run</button></form>\n";

    if (getParam(request, "run", "") != "sequence")
    {
        return html.str();
    }

    SequenceResult result = runRandomSequence(settings);

    std::vector<std::string> labels;
    ChartSeries normalPercent = makeSeries(NORMAL_NAME, NORMAL_COLOR);
    ChartSeries consistentPercent = makeSeries(CONSISTENT_NAME, CONSISTENT_COLOR);
    ChartSeries boundedPercent = makeSeries(BOUNDED_NAME, BOUNDED_COLOR);

    for (size_t i = 0; i < result.steps.size(); i++)
    {
        labels.push_back(result.steps[i].label);
        normalPercent.values.push_back(result.steps[i].modulo.percentKeysMoved);
        consistentPercent.values.push_back(result.steps[i].consistent.percentKeysMoved);
        boundedPercent.values.push_back(result.steps[i].bounded.percentKeysMoved);
    }

    html << "<h3>Result after " << result.steps.size() << " changes</h3>\n";
    html << "<table>" << methodHeaderRow()
         << "<tr><td>Total keys moved</td><td>" << result.totalModuloKeys << "</td><td>"
         << result.totalConsistentKeys << "</td><td>" << result.totalBoundedKeys << "</td></tr>\n"
         << "<tr><td>Total data moved</td><td>" << formatDataSize(result.totalModuloKB) << "</td><td>"
         << formatDataSize(result.totalConsistentKB) << "</td><td>" << formatDataSize(result.totalBoundedKB)
         << "</td></tr>\n"
         << "</table>\n";

    html << drawLineChart("Keys moved in each step", labels,
                          threeSeries(normalPercent, consistentPercent, boundedPercent), "% of all keys");
    html << "<p>(+ means a server was added, - means a server was removed)</p>\n";

    return html.str();
}

static std::string buildVnodeSection(const HttpRequest& request)
{
    int keyCount = textToInt(getParam(request, "vkeys", ""), 20000, 100, 100000);
    int nodeCount = textToInt(getParam(request, "vnodecount", ""), 5, 1, 40);
    int seed = textToInt(getParam(request, "vseed", ""), 3, 1, 99999);

    std::ostringstream html;
    html << "<h2>Test 2: effect of virtual nodes</h2>\n"
         << "<p>The same cluster is built with 1, 2, 5 ... 500 virtual nodes per server. "
         << "For each case the busiest server is compared with the average. "
         << "Each value is the average of 5 runs.</p>\n"
         << "<form method='get' action='/experiments'><input type='hidden' name='run' value='vnodes'>"
         << "Keys: <input type='number' name='vkeys' value='" << keyCount << "'> "
         << "Servers: <input type='number' name='vnodecount' value='" << nodeCount << "'> "
         << "Seed: <input type='number' name='vseed' value='" << seed << "'> "
         << "<button type='submit'>Run</button></form>\n";

    if (getParam(request, "run", "") != "vnodes")
    {
        return html.str();
    }

    VnodeTestResult result = runVnodeTest(keyCount, nodeCount, seed);

    std::vector<std::string> labels;
    ChartSeries normalExtra = makeSeries(NORMAL_NAME, NORMAL_COLOR);
    ChartSeries consistentExtra = makeSeries(CONSISTENT_NAME, CONSISTENT_COLOR);
    ChartSeries boundedExtra = makeSeries(BOUNDED_NAME, BOUNDED_COLOR);

    for (size_t i = 0; i < result.rows.size(); i++)
    {
        labels.push_back(intToText(result.rows[i].vnodes));
        normalExtra.values.push_back(result.moduloBusiestExtraPercent);
        consistentExtra.values.push_back(result.rows[i].busiestExtraPercent);
        boundedExtra.values.push_back(result.rows[i].boundedExtraPercent);
    }

    html << drawLineChart("Busiest server: % above average (lower is better)", labels,
                          threeSeries(normalExtra, consistentExtra, boundedExtra), "% above average");
    html << "<p>Bottom axis: virtual nodes per server. Normal hashing does not use virtual nodes, "
         << "so its line stays flat. The load limit keeps the busiest server at most "
         << decimalToText(LOAD_LIMIT * 100, 0) << "% above average, even with very few virtual nodes.</p>\n";

    html << "<p>Busiest server, % above average:</p>\n"
         << "<table><tr><th>Virtual nodes per server</th><th>" << NORMAL_NAME << "</th><th>" << CONSISTENT_NAME
         << "</th><th>" << BOUNDED_NAME << "</th></tr>\n";
    for (size_t i = 0; i < result.rows.size(); i++)
    {
        html << "<tr><td>" << result.rows[i].vnodes << "</td>"
             << "<td>" << decimalToText(result.moduloBusiestExtraPercent, 1) << "%</td>"
             << "<td>" << decimalToText(result.rows[i].busiestExtraPercent, 1) << "%</td>"
             << "<td>" << decimalToText(result.rows[i].boundedExtraPercent, 1) << "%</td></tr>\n";
    }
    html << "</table>\n";

    return html.str();
}

std::string buildExperimentsPage(const HttpRequest& request)
{
    std::string html = pageStart("Random test");
    html += buildSequenceSection(request);
    html += "<hr>\n";
    html += buildVnodeSection(request);
    html += pageEnd();
    return html;
}

// ---------------------------------------------------------------------------
// Explanation page ("/notes")
// ---------------------------------------------------------------------------

static const char* NOTES_BODY = R"HTML(
<h2>The problem</h2>
<p>A large database is split over several servers (this is called sharding). For every key there
must be a rule that decides which server stores it. When a server is added or removed, some keys get
a new server and their data has to be copied over the network. Copying is slow, so a good rule moves
as few keys as possible.</p>

<h2>1. Normal hashing</h2>
<pre>server number = hash(key) % number_of_servers</pre>
<p>Easy to write and spreads keys evenly. The problem is that the number of servers is part of the
formula. Going from 4 to 5 servers changes <code>hash % 4</code> into <code>hash % 5</code>, and the
two give the same answer for only about 1 in 5 keys. So about 80% of all keys move, even though the
new server only needs 20% of them.</p>
<p>Code: <code>src/modulo_sharding.cpp</code>, function <code>getNodeForKey</code>.</p>

<h2>2. Consistent hashing</h2>
<p>All hash values (0 to 4,294,967,295) are placed on a circle, called the ring. Each server is also
hashed to a point on the ring. A key belongs to the first server point found when moving clockwise
from the key.</p>
<pre>
position = hash(key)
owner    = first server point at or after position   (std::map::lower_bound)
if none  : go back to the first point on the ring     (ring.begin())
</pre>
<p>When a server is added, it only takes keys from the part of the ring just before its point.
When a server is removed, only its own keys move to the next point. All other keys stay where they are.
So only about 1/N of the keys move.</p>
<p>Code: <code>src/consistent_hashing.cpp</code>, function <code>getNodeForKey</code>.</p>

<h2>3. Virtual nodes</h2>
<p>With only one point per server, the gaps between points are random, so one server can get a much
bigger part of the ring than another. To fix this, each server is placed on the ring many times by
hashing names like <code>node-3#vn0</code>, <code>node-3#vn1</code>, <code>node-3#vn2</code> and so on.
These extra points are the virtual nodes. With many small parts per server, the total for each server
becomes close to equal.</p>
<p>Test 2 on the "Random test" page shows this with real numbers.</p>

<h2>4. Modification: consistent hashing + load limit</h2>
<p>Even with virtual nodes, one server can still end up with clearly more keys than the others,
unless a very large number of virtual nodes is used (more memory). The modification fixes this with
two ideas from data structures:</p>
<ul>
<li><b>Capacity (counter in a map):</b> every server may hold at most
<code>average &times; 1.10</code> keys. A map from server name to key count tracks how full each server is.</li>
<li><b>Linear probing:</b> a key starts at its normal place on the ring. If that server is already full,
the key moves on to the next server point clockwise, and so on, until a server with free space is found.
This is the same method that open addressing hash tables use when a slot is already taken.</li>
</ul>
<pre>
spot = ring.lower_bound(hash(key))         (binary search, same as before)
while load[spot] &gt;= capacity:             (linear probing)
    spot = next point clockwise
load[spot] = load[spot] + 1
</pre>
<p>Result: the busiest server is never more than 10% above average, even with only a few virtual nodes,
while the number of keys moved on a change stays close to normal consistent hashing.</p>
<p>Code: <code>src/bounded_hashing.cpp</code>, function <code>assignWithLoadLimit</code>.</p>

<h2>Numbers shown on the pages</h2>
<table>
<tr><td>Keys moved</td><td>keys that are on a different server after the change</td></tr>
<tr><td>Data moved</td><td>total size of those keys, i.e. what must be copied over the network</td></tr>
<tr><td>Smallest possible movement</td><td>keys / new server count when adding, keys / old server count when removing</td></tr>
<tr><td>% above average</td><td>how many more keys the busiest server has compared to the average server (lower is better)</td></tr>
</table>

<h2>Source files</h2>
<pre>
hashing.cpp            hash function used by both methods
modulo_sharding.cpp    normal hashing (hash % N)
consistent_hashing.cpp hash ring with virtual nodes
bounded_hashing.cpp    consistent hashing + load limit (linear probing)
dataset.cpp            creates test keys with sizes
metrics.cpp            counts moved keys / data, keys per server
cluster.cpp            runs both methods on the same data and compares before/after
experiments.cpp        random test and virtual node test
svg_charts.cpp         draws the charts and the ring as SVG
web_server.cpp         small HTTP server
pages.cpp              builds the HTML pages
main.cpp               handles page requests, keeps the cluster in memory
</pre>
)HTML";

std::string buildNotesPage()
{
    std::string html = pageStart("Explanation");
    html += NOTES_BODY;
    html += pageEnd();
    return html;
}
