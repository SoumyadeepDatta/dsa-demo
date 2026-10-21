# Normal Hashing vs Consistent Hashing vs Consistent Hashing with Load Limit

A C++ project that compares three ways of splitting data over several servers
(sharding):

- **Normal hashing**: `server = hash(key) % number_of_servers`
- **Consistent hashing**: a hash ring with virtual nodes
- **Consistent hashing + load limit** (the modification): the same ring, but
  every server has a capacity, and a key that lands on a full server moves on
  to the next server clockwise (linear probing, as in open addressing hash tables)

The program runs a small web server. In the browser, servers can be added and
removed, and the page shows how many keys had to move to another server and
how much data that is. A second page runs random tests automatically.

Everything is written in C++: the hashing, the web server, the HTML pages and
the charts (drawn as SVG). No external libraries are used.

New to hashing or sharding? Start with **[PROJECT_GUIDE.md](PROJECT_GUIDE.md)**:
it explains the ideas from zero, the extra C++ used, the order to read the
files in, and each algorithm with worked examples.

## Build and run

From bash (Git Bash on Windows, or a Linux / macOS terminal):

```
bash run.sh
```

This builds the program and starts it. Then open http://localhost:8080.
Stop it with Ctrl+C. Another port can be given like this: `bash run.sh 9000`.
To only build: `bash build.sh`.

Other options: `build.bat` from cmd/PowerShell, or `make`.

**"Permission denied" on Windows:** this happens when the program is rebuilt
while an old `sharding_demo.exe` is still running, because Windows locks a
running program. `build.sh` and `run.sh` stop the old copy first. To do it
by hand in Git Bash: `taskkill //F //IM sharding_demo.exe`.

## Pages

**Manual test** (`/`)
- add a server or remove any server
- start again with a different number of keys, servers or virtual nodes
- after each change the page shows:
  - keys moved and data moved for all three methods
  - a few example keys that moved
  - a chart of keys moved in every change so far
  - a chart of total data moved
  - keys on each server
  - a drawing of the hash ring

**Random test** (`/experiments`)
1. Random adding and removing of servers, with the keys moved in each step.
2. Effect of virtual nodes: the same cluster with 1 to 500 virtual nodes per
   server, showing how much more the busiest server holds than the average.
   The load limit version stays at 10% even with 1 virtual node.

**Explanation** (`/notes`): a short description of all three methods.

## The three methods in code

Normal hashing, in `src/modulo_sharding.cpp`:

```cpp
unsigned int hashValue = hashString(key);
unsigned int serverIndex = hashValue % nodeNames.size();
return nodeNames[serverIndex];
```

Consistent hashing, in `src/consistent_hashing.cpp`. The ring is a
`std::map<unsigned int, std::string>` (ring position -> server name):

```cpp
unsigned int keyPosition = hashString(key);
owner = ring.lower_bound(keyPosition);   // first server point clockwise
if (owner == ring.end())
    owner = ring.begin();                // go back to the start of the ring
return owner->second;
```

Each server is placed on the ring many times by hashing `"node-3#vn0"`,
`"node-3#vn1"`, and so on. These points are the virtual nodes.

Consistent hashing + load limit, in `src/bounded_hashing.cpp`:

```cpp
capacity = ceil(average_keys_per_server * 1.10);

spot = points.lower_bound(hashString(key));    // binary search, same as above
if (spot == points.end()) spot = points.begin();
while (load[spot->second] >= capacity)         // linear probing:
{                                              // server full -> try next point
    ++spot;
    if (spot == points.end()) spot = points.begin();
}
load[spot->second]++;
```

DSA concepts used in the modification:
- **ordered map / binary search tree** (`std::map`) for the ring, `lower_bound` in O(log V)
- **linear probing** (from open addressing hash tables) to skip full servers
- **counter map** (server -> number of keys) to know how full each server is

## Results

Measured with 10,000 keys, 4 servers and 20 virtual nodes per server, then
adding a 5th server:

| | normal hashing | consistent hashing | consistent + load limit |
|---|---|---|---|
| keys moved (smallest possible: 20%) | 80.4% | 16.1% | 21.3% |
| busiest server above average | 3.2% | 37.9% | **10.0%** |

Effect of virtual nodes (5 servers, average of 5 runs), busiest server above average:

| virtual nodes | consistent hashing | consistent + load limit |
|---|---|---|
| 1 | 141.8% | 10.0% |
| 10 | 40.0% | 10.0% |
| 50 | 16.8% | 10.0% |

So the modification:
- keeps the busiest server at most 10% above average, with any number of virtual nodes
- needs far fewer virtual nodes for good balance, so the ring uses less memory
- still moves only about 1/N of the keys on a change, a little more than plain
  consistent hashing (around 3% more in the random test), and far less than normal hashing

## Files

| file | purpose |
|------|---------|
| `src/hashing.cpp` | hash function (FNV-1a plus bit mixing), used by both methods |
| `src/modulo_sharding.cpp` | normal hashing |
| `src/consistent_hashing.cpp` | hash ring with virtual nodes |
| `src/bounded_hashing.cpp` | consistent hashing + load limit (linear probing) |
| `src/dataset.cpp` | creates test keys, each with a size in KB |
| `src/metrics.cpp` | counts moved keys and moved data, keys per server |
| `src/cluster.cpp` | runs all three methods on the same data, compares before and after |
| `src/experiments.cpp` | random test and virtual node test |
| `src/svg_charts.cpp` | bar chart, line chart and ring drawing as SVG |
| `src/web_server.cpp` | small HTTP server on sockets (Windows and Linux) |
| `src/pages.cpp` | builds the HTML pages |
| `src/main.cpp` | handles page requests, keeps the cluster in memory |
| `src/text_utils.cpp` | number formatting, HTML escaping, URL decoding |

## Limits

- One cluster is kept in memory and is reset when the program restarts.
- The server handles one request at a time, which is fine for a local demo.
- It only listens on 127.0.0.1, so it can't be reached from other machines.
