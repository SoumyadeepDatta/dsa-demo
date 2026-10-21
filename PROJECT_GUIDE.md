# Project Guide: How to Read and Understand This Project

This guide explains the whole project from zero. It assumes only basic C++
(variables, loops, `if`/`else`) and no knowledge of hashing, sharding or
web servers.

Suggested way to use it:

1. Read **Part 1** (the ideas) completely before opening any code.
2. Read **Part 2** (the extra C++ needed) and keep it open as a reference.
3. Follow **Part 3** (the reading order) file by file.
4. Use **Part 4** (the algorithms in detail) while reading the three
   algorithm files.
5. Finish with **Parts 5 to 8** to see how everything is connected, and check
   understanding with the questions in **Part 9**.

---

## Contents

- [Part 1: The ideas, from zero](#part-1-the-ideas-from-zero)
- [Part 2: C++ needed beyond the basics](#part-2-c-needed-beyond-the-basics)
- [Part 3: Reading order](#part-3-reading-order)
- [Part 4: The algorithms in detail](#part-4-the-algorithms-in-detail)
- [Part 5: How the measurements work](#part-5-how-the-measurements-work)
- [Part 6: Life of one button click](#part-6-life-of-one-button-click)
- [Part 7: The random tests](#part-7-the-random-tests)
- [Part 8: Complexity summary](#part-8-complexity-summary)
- [Part 9: Questions to check understanding](#part-9-questions-to-check-understanding)
- [Part 10: Glossary](#part-10-glossary)

---

## Part 1: The ideas, from zero

### 1.1 Keys and values

Many databases store data as **key → value** pairs, like a dictionary:

| key | value |
|---|---|
| `user:1` | name = "Asha", city = "Pune", ... |
| `order:2` | items = [...], total = 1200 |
| `cart:3` | ... |

The **key** is a short name used to find the data. The **value** is the data
itself. In this project the values are not real; each key just has a **size
in KB**, because the size is what matters when data has to be copied.

### 1.2 Why split data over many servers (sharding)

One computer has limited disk, memory and speed. When an app grows to
millions of users, one server is not enough. The solution is to use many
servers and give each server **a part** of the keys:

```
             all keys
   +-----------+-----------+-----------+
   |           |           |           |
 server 1    server 2    server 3    server 4
 (1/4 of     (1/4 of     (1/4 of     (1/4 of
  the keys)   the keys)   the keys)   the keys)
```

This is called **sharding**. Each part is a **shard**.

Sharding needs a **rule** that answers one question:

> Given a key, which server stores it?

Every program that reads or writes data must use the same rule, otherwise it
looks for a key on the wrong server.

### 1.3 What a good rule needs

1. **Same answer every time** for the same key (no randomness).
2. **Even spread**: every server should get about the same number of keys.
   If one server gets twice as many keys, it needs twice the disk and gets
   twice the traffic.
3. **Few moves when servers change**: servers are added (more users) and
   removed (a machine breaks). When that happens, some keys get a new server
   and their data must be **copied over the network**. Copying is slow and
   expensive, so a good rule moves as few keys as possible.

This project compares three rules against exactly these three points.

### 1.4 What a hash function is

A **hash function** turns any text into a number. The project's hash function
gives numbers between 0 and 4,294,967,295 (that is 2³² − 1, the largest value
of a 32-bit `unsigned int`). Real values from the project's own function:

| text | hash value |
|---|---|
| `user:1` | 1763983599 |
| `order:2` | 1496151174 |
| `cart:3` | 1703277056 |
| `session:4` | 415424890 |
| `user:5` | 2633409956 |

Important properties:

- **Deterministic**: `user:1` always gives 1763983599.
- **Looks random**: `user:1` and `user:5` are almost the same text, but their
  hashes are completely different. This spreads keys evenly.
- **Fast**: only a few operations per character.

A hash function is useful for sharding because it turns a key into a number,
and a number can be turned into a server.

### 1.5 The three rules compared in this project

**Rule 1: Normal hashing**

```
server number = hash(key) % number_of_servers
```

`%` is the remainder after division. With 4 servers, the remainder is always
0, 1, 2 or 3, which is a server number. Simple and very even. The problem:
when the number of servers changes, almost every key gets a different
remainder, so almost all data must move (about 80% when going from 4 to 5
servers).

**Rule 2: Consistent hashing**

Imagine all possible hash values arranged on a circle (the **ring**). Each
server is also hashed to a point on the circle. A key belongs to the **first
server found when walking clockwise** from the key's point. When a server is
added, only the keys just before it change owner; all others stay. Only
about 1/N of keys move (N = number of servers). To spread keys evenly, each
server is put on the ring many times; these extra points are called
**virtual nodes**.

**Rule 3: Consistent hashing + load limit (the modification)**

Same ring, with one extra rule borrowed from hash tables: every server has a
maximum number of keys (**capacity** = average × 1.10). If a key's server is
already full, the key keeps walking clockwise to the next server that has
room. This walking is called **linear probing**. Result: no server is ever
more than 10% above average, while the number of moved keys stays small.

Part 4 explains all three in detail with worked examples.

### 1.6 What the program does

The program is a small **web server** written in C++. When it runs, a
browser can open `http://localhost:8080` and:

- add or remove servers by clicking buttons,
- see how many keys each rule moved and how much data that is,
- see charts comparing the rules,
- run random tests automatically.

All three rules always work on the **same keys** and the **same list of
servers**, so the comparison is fair.

---

## Part 2: C++ needed beyond the basics

The code uses only a few things beyond loops, variables and conditions. Each
is explained here with a tiny example.

### 2.1 Functions

A function is a named piece of code that takes inputs and returns a result.

```cpp
int addTen(int number)      // takes an int, gives back an int
{
    return number + 10;
}

int x = addTen(5);          // x is 15
```

`void` means the function returns nothing.

### 2.2 `std::string`

Text. Can be joined with `+` and compared with `==`.

```cpp
std::string name = "node-" + std::string("3");   // "node-3"
if (name == "node-3") { ... }
name.size();                                      // number of characters: 6
name[0];                                          // first character: 'n'
```

### 2.3 `std::vector` (a growable array)

```cpp
std::vector<int> numbers;      // empty list of ints
numbers.push_back(7);          // [7]
numbers.push_back(9);          // [7, 9]
numbers[1];                    // 9
numbers.size();                // 2
numbers.erase(numbers.begin() + 0);   // remove element 0 -> [9]
```

Used everywhere: list of servers, list of keys, list of owners.

### 2.4 `std::map` (sorted key → value table)

```cpp
std::map<std::string, int> load;   // server name -> number of keys
load["node-1"] = 0;
load["node-1"]++;                  // now 1
```

Two facts about `std::map` matter a lot in this project:

1. It keeps its entries **sorted by key**, always.
2. Inside, it is a **balanced binary search tree** (usually a red-black
   tree). Finding, inserting and deleting take **O(log n)** time.

The function `lower_bound(x)` returns the **first entry whose key is ≥ x**.
It uses the tree to find it in O(log n). This single function is the heart
of consistent hashing (Part 4.3).

```cpp
std::map<unsigned int, std::string> ring;
ring[20] = "A";  ring[50] = "B";  ring[80] = "C";

ring.lower_bound(35)   // -> the entry (50, "B")
ring.lower_bound(50)   // -> the entry (50, "B")   (equal counts)
ring.lower_bound(90)   // -> ring.end()  (nothing is >= 90)
```

### 2.5 Iterators

An **iterator** is like a pointer to one entry inside a container.

```cpp
std::map<unsigned int, std::string>::const_iterator spot = ring.lower_bound(35);
spot->first     // the key   (50)
spot->second    // the value ("B")
++spot;         // move to the next entry (80, "C")
spot == ring.end()    // true when moved past the last entry
ring.begin()          // the first entry (20, "A")
```

The long type name `std::map<unsigned int, std::string>::const_iterator` just
means "an iterator into this kind of map that is only used for reading".

### 2.6 `struct`

A `struct` groups several variables into one.

```cpp
struct DataRecord
{
    std::string key;
    int sizeKB;
};

DataRecord r;
r.key = "user:1";
r.sizeKB = 12;
```

### 2.7 `class`

A `class` is a `struct` that also has functions (called **member functions**
or **methods**) and can hide its data (`private`).

```cpp
class Counter
{
public:                         // anyone can call these
    void add() { count++; }
    int get() { return count; }
private:                        // only the class itself can touch this
    int count;
};
```

In the `.cpp` files, member functions are written as `ClassName::function`:

```cpp
void ModuloSharding::addNode(const std::string& nodeName) { ... }
```

### 2.8 References and `const`

```cpp
void show(const std::string& text)
```

- `&` means **reference**: the function works on the original variable, no
  copy is made. This matters for big things like a vector of 100,000 keys.
- `const` means the function **promises not to change** it.

A function marked `const` at the end (like `getNodeForKey(...) const`)
promises not to change its object.

### 2.9 Header files (`.h`) and source files (`.cpp`)

Every part of the project has two files:

- `name.h` — the **declaration**: what exists (structs, classes, function
  names). Like a table of contents.
- `name.cpp` — the **implementation**: the actual code.

Other files use `#include "name.h"` to get access. These lines in every header:

```cpp
#ifndef HASHING_H
#define HASHING_H
...
#endif
```

are an **include guard**: they stop the same header from being included twice.

**Tip:** always read the `.h` first (it is short and says *what*), then the
`.cpp` (it says *how*).

### 2.10 `unsigned int` and overflow

An `unsigned int` holds 0 to 4,294,967,295 (32 bits). If a calculation goes
above that, it **wraps around** to the start, like a car's odometer:

```
4,000,000,000 + 500,000,000  =  205,032,704   (wrapped around once)
```

For normal programs this is usually a bug, but for hashing it is **useful**:
the multiplications in the hash function are allowed to wrap, which mixes the
bits. The ring code also uses it (Part 4.3).

### 2.11 Bit operators used in the hash function

| operator | name | meaning |
|---|---|---|
| `a ^ b` | XOR | each bit is 1 if exactly one of the two bits is 1 |
| `a >> 16` | right shift | move all bits 16 places right (like dividing by 2¹⁶) |

Small XOR example on 4 bits: `1100 ^ 1010 = 0110`.

These are only used inside `hashString()`. It is fine to treat that function
as a black box at first.

### 2.12 `std::ostringstream` (building text)

Used to build HTML and SVG text piece by piece:

```cpp
std::ostringstream out;
out << "Keys: " << 1500 << " moved";
std::string text = out.str();      // "Keys: 1500 moved"
```

### 2.13 `static` (two meanings in this code)

- `static` in front of a function in a `.cpp` file: the function is only
  used inside that file (a private helper).
- `static` in front of a global variable in `main.cpp`: the variable lives for
  the whole program run (the cluster is kept between page loads).

### 2.14 Raw strings `R"CSS( ... )CSS"`

A way to write a long multi-line text in C++ without escaping quotes. Used for
the small CSS block and the explanation page text in `pages.cpp`.

---

## Part 3: Reading order

The files are listed in the order they should be read. Each step builds on
the previous one. Time estimates are rough.

```
            hashing  ───────────────────────────────┐
               │                                    │
     ┌─────────┴───────────┐                        │
modulo_sharding    consistent_hashing               │   (the 3 algorithms)
                           │                        │
                    bounded_hashing ────────────────┘
                           │
dataset ── metrics ── cluster          (runs all 3 and measures them)
                           │
                     experiments       (automatic random tests)
                           │
main ── web_server ── pages ── svg_charts ── text_utils   (the website)
```

### Step 0: `README.md` (5 min)

Skim it to see what the project does. Do not worry about details yet.

Optional but helpful: build and run the program (`bash run.sh`), open
`http://localhost:8080`, add and remove a few servers, and look at the
numbers. Seeing the result first makes the code much easier to follow.

### Step 1: `src/hashing.h` and `src/hashing.cpp` (10 min)

**What it is:** the single hash function `hashString(text)` used by all three
rules.

**Look for:**
- the loop over characters (FNV-1a),
- the five "finalizer" lines after the loop.

**Takeaway:** text in, 32-bit number out, same text always gives the same
number. Details in [Part 4.1](#41-the-hash-function-hashstring).

### Step 2: `src/modulo_sharding.h` and `.cpp` (10 min)

**What it is:** Rule 1, normal hashing.

**Look for:** `getNodeForKey()`. The whole algorithm is three lines:
hash, `%`, pick from the vector.

**Also notice:** `removeNode()` erases from the middle of the vector, which
shifts every later server to a new index. This is one reason so many keys
move. Details in [Part 4.2](#42-rule-1-normal-hashing).

### Step 3: `src/consistent_hashing.h` and `.cpp` (30 min)

**What it is:** Rule 2, the hash ring with virtual nodes.

**Read in this order inside the file:**
1. The long comment at the top of the `.h` file (the idea in words).
2. `makeVirtualNodeLabel()` — builds names like `node-2#vn5`.
3. `placeVirtualNodes()` — puts one server's points on the ring.
4. `getNodeForKey()` — **the most important function in the project**:
   `lower_bound` + wrap around.
5. `addNode()` and `removeNode()`.
6. `getRingOwnershipPercent()` — how much of the circle each server owns.

Details in [Part 4.3](#43-rule-2-consistent-hashing) and
[Part 4.4](#44-virtual-nodes).

### Step 4: `src/bounded_hashing.h` and `.cpp` (20 min)

**What it is:** Rule 3, the modification (load limit with linear probing).

**Look for:**
- Step 1: capacity calculation with `ceil()`,
- Step 2: the `load` map (counter per server),
- Step 3: the same `lower_bound` start as Rule 2, followed by the `while`
  loop that skips full servers.

**Compare** it side by side with `ConsistentHashRing::getNodeForKey()`. The
only new part is the `while` loop and the counter. Details in
[Part 4.5](#45-rule-3-consistent-hashing--load-limit-the-modification).

### Step 5: `src/dataset.h` and `.cpp` (5 min)

**What it is:** creates the fake keys (`user:1`, `order:2`, ...) with sizes.

**Look for:** `srand(seed)` makes the "random" data the same every time for
the same seed, so results can be repeated.

### Step 6: `src/metrics.h` and `.cpp` (15 min)

**What it is:** the measuring code.

**Look for:**
- `compareOwners()` — compares "who owned each key before" with "who owns it
  after" and counts the differences.
- `measureLoad()` — counts keys per server, finds the busiest server and
  computes "% above average".

Details in [Part 5](#part-5-how-the-measurements-work).

### Step 7: `src/cluster.h` and `.cpp` (20 min)

**What it is:** the class `ShardedCluster` that runs **all three rules at the
same time** on the same keys and the same servers.

**Look for:**
- the members `moduloOwners`, `ringOwners`, `boundedOwners`: three vectors,
  where `owners[i]` is the server that holds key number `i`,
- `computeAllOwners()` — asks each rule for the owner of every key,
- `addNode()` and `removeNode()` — both follow the same 4 steps written in
  the comment at the top of the file: remember → change → recompute → compare.

This file connects the algorithms (Steps 1–4) with the measurements (Step 6).

### Step 8: `src/experiments.h` and `.cpp` (15 min)

**What it is:** the two automatic tests on the "Random test" page.

**Look for:**
- `runRandomSequence()` — random add/remove steps, records each result,
- `runVnodeTest()` — tries 1, 2, 5 ... 500 virtual nodes and measures balance.

Details in [Part 7](#part-7-the-random-tests).

At this point all the "DSA" part of the project has been read. The remaining
files are about showing results in a browser.

### Step 9: `src/main.cpp` (10 min)

**What it is:** the starting point of the program (`main()`) and the
"router" `handleRequest()` that decides what to do for each web address.

**Look for:** the `if (request.path == "/add" ...)` blocks. Each one calls
into `ShardedCluster` and then either returns a page or redirects to `/`.

### Step 10: `src/web_server.h` and `.cpp` (20 min, can be skimmed)

**What it is:** a very small web server written directly on top of
**sockets** (the operating system's network connections).

**Look for:** the comment at the top (how a browser and server talk), then
`runServer()`: create socket → bind to port → listen → loop forever
(accept → read request → `handleRequest` → send response → close).

Knowing every line here is not needed to understand the algorithms.

### Step 11: `src/pages.h` and `.cpp` (20 min, can be skimmed)

**What it is:** builds the HTML text of each page.

**Look for:** `buildClusterPage()` and the smaller `build...` functions it
calls (servers table, last change, history charts, keys per server, ring).
HTML is plain text, so these functions just glue strings together.

### Step 12: `src/svg_charts.h` and `.cpp` (20 min, optional)

**What it is:** draws bar charts, line charts and the ring as **SVG**. SVG is
a text format for pictures (`<rect>`, `<line>`, `<circle>`, `<text>`) that
browsers can display.

**Look for:** `valueToY()` (turns a number into a pixel height) and
`drawHashRing()` (turns ring positions into angles on a circle).

### Step 13: `src/text_utils.h` and `.cpp` (5 min, optional)

Small helpers: number to text, HTML escaping, URL decoding.

### Step 14: build files (2 min)

`build.sh`, `run.sh`, `build.bat`, `Makefile` — all simply run
`g++ -std=c++11 ... src/*.cpp` (plus `-lws2_32` on Windows for sockets).

---

## Part 4: The algorithms in detail

### 4.1 The hash function `hashString()`

File: `src/hashing.cpp`

```cpp
unsigned int hash = 2166136261u;
for (size_t i = 0; i < text.size(); i++)
{
    unsigned char character = (unsigned char)text[i];
    hash = hash ^ character;
    hash = hash * 16777619u;
}
hash = hash ^ (hash >> 16);
hash = hash * 0x85ebca6bu;
hash = hash ^ (hash >> 13);
hash = hash * 0xc2b2ae35u;
hash = hash ^ (hash >> 16);
return hash;
```

**Part A: FNV-1a (the loop).** FNV-1a is one of the simplest well-known hash
functions.

1. Start with a fixed number: 2166136261.
2. For each character:
   - XOR the character's code into the hash (this "adds" the character),
   - multiply by 16777619 (this spreads that change to many bits; the
     result is allowed to wrap around, see Part 2.10).

Example, first step for text starting with `a` (character code 97):

```
start:            2166136261
XOR with 97:      2166136228
times 16777619:   3826002220   (after wrap-around)
```

Each next character repeats the same two operations.

**Part B: the finalizer (the last 5 lines).** FNV-1a alone gives similar
results for texts that differ only in the last character, like `node-1#vn1`
and `node-1#vn2`. On the ring, similar numbers would mean virtual nodes
bunched together, which is bad for balance. The finalizer (taken from the
well-known MurmurHash3) shifts, XORs and multiplies a few more times, so that
changing one character changes about half of the output bits.

**Why both rules use the same hash function:** to make the comparison fair.
The only difference between the rules is what is done with the hash number.

**Time:** O(L) where L is the length of the text.

---

### 4.2 Rule 1: Normal hashing

File: `src/modulo_sharding.cpp`, function `getNodeForKey()`

```cpp
unsigned int hashValue = hashString(key);
unsigned int serverIndex = hashValue % (unsigned int)nodeNames.size();
return nodeNames[serverIndex];
```

**How it works:**

1. Hash the key to a big number.
2. Take the remainder after dividing by the number of servers. With N
   servers the remainder is always between 0 and N−1.
3. Use that as an index into the list of servers.

**Worked example with real values (4 servers, then 5 servers):**

| key | hash | hash % 4 (4 servers) | hash % 5 (5 servers) | moved? |
|---|---|---|---|---|
| `user:1` | 1763983599 | 3 | 4 | yes |
| `order:2` | 1496151174 | 2 | 4 | yes |
| `cart:3` | 1703277056 | 0 | 1 | yes |
| `session:4` | 415424890 | 2 | 0 | yes |
| `user:5` | 2633409956 | 0 | 1 | yes |

All five keys changed server, just because one server was added.

**Why about 80% move when going from 4 to 5 servers:**

A key stays only if `hash % 4 == hash % 5`. Remainders by 4 and by 5 repeat
together every 20 numbers (4 × 5 = 20). Out of each 20 consecutive numbers,
only 0, 1, 2 and 3 give the same remainder for both. That is 4 out of 20 =
20% that stay, so **80% move**. Going from 10 to 11 servers: 10 out of 110
stay, so about **91% move**. The more servers, the worse it gets.

But the new 5th server only *needs* 1/5 = 20% of the keys to be balanced.
So normal hashing moves four times more data than necessary.

**Removing a server is just as bad:** removing `node-2` from
`[node-1, node-2, node-3, node-4]` gives `[node-1, node-3, node-4]`. Now N
changes from 4 to 3, **and** `node-3` and `node-4` shift to new indexes.

**Strengths:** very simple, O(1) lookup, almost perfectly even spread.

**Weakness:** almost everything moves whenever N changes.

---

### 4.3 Rule 2: Consistent hashing

File: `src/consistent_hashing.cpp`

#### The idea: a circle of numbers

All hash values 0 … 4,294,967,295 are imagined as points on a circle. After
the largest value comes 0 again, like on a clock where 12 is followed by 1.

For the examples below, a small circle with positions **0 to 99** is used
instead of 0 to 4 billion, so the numbers are easy to follow.

#### Step 1: place servers on the circle

Each server's name is hashed to get its position. Example:

```
server A → position 20
server B → position 50
server C → position 80
```

```
                 0
             95  .  5
          90  .     .  10
       85  .           .  15
     C=80 *               * A=20
       75  .           .  25
          70  .     .  30
             65  .  35
                 50=B *     (B at the bottom)
```

#### Step 2: find the owner of a key

Hash the key to a position, then **walk clockwise until the first server**.

| key position | first server clockwise | owner |
|---|---|---|
| 35 | B (50) | B |
| 50 | B (50) | B (equal counts as found) |
| 70 | C (80) | C |
| 90 | none until 99 → wrap to start → A (20) | A |

So each server owns the **arc that ends at it**:

- A owns 81 … 20 (wrapping over 0),
- B owns 21 … 50,
- C owns 51 … 80.

#### How the code does it

The ring is stored as a `std::map<unsigned int, std::string>`:
position → server name. Because a map is always sorted, "first server
clockwise from position X" is exactly `lower_bound(X)`:

```cpp
unsigned int keyPosition = hashString(key);
std::map<unsigned int, std::string>::const_iterator owner = ring.lower_bound(keyPosition);
if (owner == ring.end())        // past the last server on the ring
{
    owner = ring.begin();       // wrap around to the first one
}
return owner->second;
```

`lower_bound` is a **binary search** inside the balanced tree, so it takes
**O(log P)** steps, where P is the number of points on the ring.

#### Step 3: what happens when a server is added

Add server D at position 65:

```
before:  B (50) .......... C (80)        keys 51..80 belong to C
after:   B (50) ... D (65) ... C (80)    keys 51..65 now belong to D
                                         keys 66..80 still belong to C
```

**Only the keys between 51 and 65 move, and they move from C to D.** Keys
owned by A and B do not change at all. Compare this with normal hashing,
where almost every key changed.

#### Step 4: what happens when a server is removed

Remove B (position 50): B's arc (21 … 50) now continues to the next server
clockwise, which is C. **Only B's keys move**, all to C. Nobody else is
affected.

#### Why only about 1/N of keys move

A new server takes over parts of the circle. If servers are spread evenly,
each owns about 1/N of the circle, so the new server takes about 1/N of the
keys. That is the smallest possible amount: the new server needs at least
that many keys to have its fair share.

#### `getRingOwnershipPercent()`: how much of the circle each server owns

For each point on the ring, the arc length is
`this position − previous position`. For the first point, the "previous" one
is the last point on the ring (wrap-around). The code uses a neat trick:
with `unsigned int`, `small − big` wraps around (Part 2.10) and gives exactly
the right arc length across the 0 point, so no special case is needed.

#### Removing a server's points quickly

`removeNode()` does not search the whole ring. A server's point positions
came from `hashString("node-2#vn0")`, `hashString("node-2#vn1")`, and so on,
so the code simply computes those positions again and erases them directly.

---

### 4.4 Virtual nodes

#### The problem with one point per server

With only one point per server, the points land at random places. Example
with 3 servers on the 0–99 circle:

```
A at 10,  B at 15,  C at 60
A owns 61..10  → 50 positions  (50%)
B owns 11..15  →  5 positions  ( 5%)
C owns 16..60  → 45 positions  (45%)
```

B gets 10 times fewer keys than A. This is very common with few points. In
the project's test with 5 servers and 1 point each, the busiest server had
on average **142% more keys** than the average server.

#### The fix: many points per server

Each server is placed on the circle many times, by hashing different labels:

```
node-1#vn0, node-1#vn1, node-1#vn2, ... node-1#vn49
node-2#vn0, node-2#vn1, ...
```

These extra points are the **virtual nodes** (vnodes). In the map, every
virtual node points back to its real server name:

```
ring[hash("node-1#vn0")] = "node-1"
ring[hash("node-1#vn1")] = "node-1"
...
```

Now each server owns **many small arcs** spread around the circle. Some arcs
are big, some small, but the total of many random arcs is close to the
average (the same reason the average of many dice throws is close to 3.5).
The unevenness shrinks roughly like 1/√V, where V = virtual nodes per server:
100 virtual nodes make it about 10 times more even than 1.

Results from the project's virtual node test (5 servers, average of 5 runs):

| virtual nodes per server | busiest server above average |
|---|---|
| 1 | 141.8% |
| 10 | 40.0% |
| 50 | 16.8% |

#### Extra benefit

When a server is removed, its many small arcs go to many different
neighbours, so its keys are shared out instead of all landing on one server.

#### Cost

- **Memory:** the ring holds N × V entries.
- **Time:** lookup is O(log(N × V)) instead of O(log N). Still very fast.

Even with 50–100 virtual nodes, the busiest server is often 10–20% above
average. Getting lower needs a lot more virtual nodes (more memory). This is
the weakness that Rule 3 fixes.

---

### 4.5 Rule 3: Consistent hashing + load limit (the modification)

File: `src/bounded_hashing.cpp`, function `assignWithLoadLimit()`

#### The idea

Keep the same ring, but give every server a **capacity** (maximum number of
keys). If a key's normal server is full, the key keeps walking clockwise
until it finds a server with free space.

This is the same idea as **linear probing** in hash tables:

| hash table with open addressing | this ring |
|---|---|
| array of slots | points on the ring |
| key hashes to a slot | key hashes to a ring position |
| slot already taken → try next slot | server already full → try next point |
| wrap to slot 0 at the end of the array | wrap to `ring.begin()` at the end of the ring |

#### DSA concepts used

1. **Balanced binary search tree** (`std::map`) + **binary search**
   (`lower_bound`) to find the starting point — same as Rule 2.
2. **Linear probing** to skip servers that are full.
3. **Counter map** (`std::map<std::string, int> load`) to know how many keys
   each server already has.

#### The code, step by step

**Step 1: capacity**

```cpp
double average = (double)records.size() / serverCount;
int capacity = (int)std::ceil(average * (1.0 + LOAD_LIMIT));   // LOAD_LIMIT = 0.10
```

Example: 10,000 keys and 4 servers → average 2,500 → capacity
`ceil(2,750.0)` = 2,750. No server may ever hold more than 2,750 keys.

`ceil` rounds up. This guarantees that all capacities together are **more**
than the number of keys (4 × 2,750 = 11,000 > 10,000), so every key is sure
to find a free server and the `while` loop always ends.

**Step 2: a counter for each server**

```cpp
std::map<std::string, int> load;     // all start at 0
```

**Step 3: place every key**

```cpp
spot = points.lower_bound(hashString(key));   // same start as Rule 2
if (spot == points.end()) spot = points.begin();

while (load[spot->second] >= capacity)        // server full?
{
    ++spot;                                   // try next point clockwise
    if (spot == points.end()) spot = points.begin();
}

owners[i] = spot->second;
load[spot->second]++;
```

Without the `while` loop, this is exactly Rule 2.

#### Worked example

Small circle 0–99, servers A at 20, B at 50, C at 80. Nine keys at positions:

```
5, 10, 15, 25, 30, 35, 40, 45, 60
```

Average = 9 / 3 = 3 keys per server. Capacity = ceil(3 × 1.10) = ceil(3.3) = **4**.

*Rule 2 (no limit):*

| key at | owner |
|---|---|
| 5, 10, 15 | A |
| 25, 30, 35, 40, 45 | B |
| 60 | C |

A = 3, **B = 5**, C = 1. B has 67% more than average.

*Rule 3 (capacity 4), keys placed in order:*

| key at | first server | its load before | result |
|---|---|---|---|
| 5 | A | 0 | A (A=1) |
| 10 | A | 1 | A (A=2) |
| 15 | A | 2 | A (A=3) |
| 25 | B | 0 | B (B=1) |
| 30 | B | 1 | B (B=2) |
| 35 | B | 2 | B (B=3) |
| 40 | B | 3 | B (B=4, now full) |
| 45 | B | 4 = full → probe next point: C, load 0 | **C** (C=1) |
| 60 | C | 1 | C (C=2) |

A = 3, **B = 4**, C = 2. The busiest server dropped from 5 to 4. (With only
9 keys, rounding makes it 33% above average. With thousands of keys, the
limit of 10% is reached exactly, as the program shows.)

#### Why keys must be placed all at once

In Rules 1 and 2, each key's server depends only on the key. In Rule 3 it
also depends on **how full the servers already are**, which depends on the
keys placed before it. So the function places all keys in one pass, always
in the same order (dataset order), which keeps the result repeatable.

#### Results in the program

10,000 keys, 4 servers, 20 virtual nodes, then adding a 5th server:

| | normal | consistent | consistent + load limit |
|---|---|---|---|
| busiest server above average | 3.2% | 37.9% | **10.0%** |
| keys moved (fair amount 20%) | 80.4% | 16.1% | 21.3% |

- **Balance:** the busiest server is at most 10% above average, with any
  number of virtual nodes, even 1.
- **Memory:** good balance no longer needs hundreds of virtual nodes.
- **Keys moved:** still about 1/N, close to the fair 20%. Plain consistent
  hashing moved only 16.1% here because its new server got *less* than its
  fair share (it was unbalanced). In the random test, Rule 3 moves about 3%
  more keys in total than Rule 2, which is a small price for the balance.

#### Trade-offs (worth mentioning in a presentation)

- Assigning keys is a little slower: after `lower_bound`, a few extra probe
  steps may be needed when servers are full.
- A small number of extra keys move when servers change, because the
  capacity changes and some keys that overflowed get placed differently.
- In real systems this technique is known as **consistent hashing with
  bounded loads** (published by Google researchers in 2016–2017 and used in
  load balancers such as HAProxy and in Vimeo's video servers).

---

## Part 5: How the measurements work

File: `src/metrics.cpp`

### 5.1 The "owners" list

For each rule, the cluster keeps a vector where `owners[i]` is the server
that holds key number `i`:

```
records:        [ user:1,  order:2,  cart:3,  ... ]
moduloOwners:   [ node-4,  node-3,   node-1,  ... ]
ringOwners:     [ node-2,  node-2,   node-4,  ... ]
boundedOwners:  [ node-2,  node-1,   node-4,  ... ]
```

### 5.2 Keys moved and data moved (`compareOwners`)

Before a change, the owners list is copied (`ownersBefore`). After the
change, it is computed again (`ownersAfter`). Then:

```
for every key i:
    if ownersBefore[i] != ownersAfter[i]:
        keysMoved   = keysMoved + 1
        dataMovedKB = dataMovedKB + size of key i
```

- **Keys moved %** = keysMoved / totalKeys × 100
- **Data moved** = sum of the sizes of the moved keys (what must be copied
  over the network). Data is measured separately from keys because some keys
  are much bigger than others.

It also saves the first few moved keys as examples (`examples` vector) to
show on the page.

### 5.3 Smallest possible movement

Set in `cluster.cpp`:

- **Adding** a server: the new server needs its fair share, so at least
  `totalKeys / newServerCount` keys must move.
- **Removing** a server: its keys (about `totalKeys / oldServerCount`) must go
  somewhere.

This is the "Smallest possible movement" line on the page.

### 5.4 Balance (`measureLoad`)

1. Count keys per server.
2. `average = totalKeys / numberOfServers`.
3. Find the busiest server (`mostKeys`).
4. **% above average** = (mostKeys − average) / average × 100.

Example: average 2,000, busiest 2,600 → 30% above average. 0% would be
perfectly even. Lower is better.

---

## Part 6: Life of one button click

What happens when "Add server" is clicked on the page:

```
Browser                         C++ program
───────                         ───────────
1. sends text:
   POST /add
   ...
   name=node-5      ─────────►  2. web_server.cpp: readRequest()
                                   turns the text into an HttpRequest
                                   (method="POST", path="/add",
                                    params["name"]="node-5")

                                3. main.cpp: handleRequest()
                                   sees path "/add", calls
                                   cluster.addNode("node-5", result, error)

                                4. cluster.cpp: addNode()
                                   a. copy the three owners lists (before)
                                   b. modulo.addNode()   ring.addNode()
                                   c. computeAllOwners()
                                      - normal: hash % N for every key
                                      - consistent: lower_bound for every key
                                      - load limit: assignWithLoadLimit()
                                   d. compareOwners() three times
                                   → OperationResult with all numbers

                                5. main.cpp saves the result in `history`
                                   and answers "303 redirect to /"
6. browser follows   ◄─────────
   the redirect:
   GET /            ─────────►  7. main.cpp → pages.cpp:
                                   buildClusterPage() builds HTML text,
                                   svg_charts.cpp draws the charts as SVG text

8. browser shows    ◄─────────  9. web_server.cpp: sendResponse()
   the new page                    sends the HTML back
```

Why the redirect (step 5–6)? If the page were returned directly after the
POST, pressing F5 (refresh) would send the POST again and add another server.
After a redirect, refresh just loads `/` again.

The page has **no JavaScript**. Every button is a normal HTML form, and every
change builds a completely new page on the server.

---

## Part 7: The random tests

File: `src/experiments.cpp`, shown on the "Random test" page.

### Test 1: random adding and removing (`runRandomSequence`)

1. Build a cluster with the chosen number of keys, servers and virtual nodes.
2. Repeat for the chosen number of steps:
   - choose add or remove (randomly, or only add / only remove),
   - keep the server count between 2 and 40,
   - do the change with `cluster.addNode()` or `cluster.removeNode()`,
   - add the moved keys and data of all three rules to the totals.
3. Show a totals table and a line chart of "% keys moved" per step.

`srand(seed)` makes the "random" choices repeatable: the same seed gives the
same sequence of changes every time.

### Test 2: effect of virtual nodes (`runVnodeTest`)

1. For each virtual node count in {1, 2, 5, 10, 20, 50, 100, 200, 500}:
   - build the cluster 5 times with different server names
     (`t0-1, t0-2, ...`, then `t1-1, t1-2, ...`). Different names give
     different ring layouts, because positions come from hashing the names;
   - measure "% above average" for each rule;
   - take the average of the 5 runs (one layout can be lucky or unlucky).
2. Show a line chart and a table.

Expected picture: normal hashing is a flat line near 2%, consistent hashing
starts very high and goes down as virtual nodes increase, and the load limit
version is a flat line at 10% from the very first point.

---

## Part 8: Complexity summary

N = servers, V = virtual nodes per server, P = N × V points on the ring,
K = keys.

| operation | normal hashing | consistent hashing | consistent + load limit |
|---|---|---|---|
| find server for one key | O(1) | O(log P) | O(log P + probes) |
| memory for the rule | O(N) | O(P) | O(P + N) |
| add a server | O(1) | O(V log P) | O(V log P) |
| keys moved on add | about K × N/(N+1) | about K/(N+1) | about K/(N+1), slightly more |
| busiest server above average | very low | depends on V | at most 10% |

"probes" is the number of full servers skipped; it is usually 0 or very small
and only grows near the end when most servers are almost full.

Note: to measure fairly, the program recomputes the owner of **every** key
after each change (O(K log P)). A real database would only look at the keys
in the affected ranges.

---

## Part 9: Questions to check understanding

Try to answer first, then read the answer.

**1. Why is `hash % N` bad when servers are added?**
Because N is part of the formula. When N changes, the remainder changes for
most keys, so about N/(N+1) of all keys move (80% for 4 → 5 servers).

**2. In consistent hashing, which keys move when a server is added?**
Only the keys on the arcs just before the new server's points. They move to
the new server. All other keys keep their server.

**3. Which C++ function finds "the first server clockwise", and how fast is it?**
`std::map::lower_bound`. It is a binary search in a balanced binary search
tree, O(log P).

**4. What happens when `lower_bound` returns `ring.end()`?**
The key's position is after the last server point, so it wraps around to
`ring.begin()`, the first point. This makes the line of numbers a circle.

**5. Why are virtual nodes needed?**
With one point per server, the arcs are random and very uneven. Many points
per server average out the arc sizes, so every server gets close to its fair
share.

**6. What DSA idea does the modification use, and where does it come from?**
Linear probing from open addressing hash tables: if the slot (server) is
full, try the next one. It also uses a counter map for the loads and the
same binary search on the ring.

**7. Why does the `while` loop in `assignWithLoadLimit()` always end?**
Capacity is rounded up with `ceil`, so all capacities added together are more
than the number of keys. There is always at least one server with room.

**8. Why does Rule 3 place all keys in one pass instead of one by one?**
Because a key's server depends on how full the servers already are, which
depends on the keys placed before it.

**9. What is the cost of the modification?**
Slightly more keys move on a change (a few percent), and lookups may need a
few extra probe steps. In return, no server is more than 10% above average.

**10. What is the difference between "keys moved" and "data moved"?**
Keys moved counts keys. Data moved adds up their sizes. A few large keys can
cost more network traffic than many small ones.

---

## Part 10: Glossary

| word | meaning |
|---|---|
| **key** | the name used to find a piece of data, e.g. `user:1` |
| **server / node** | one machine that stores part of the data |
| **sharding** | splitting data over many servers |
| **hash function** | turns text into a number; same text, same number |
| **hash value** | the number produced by the hash function |
| **modulo (`%`)** | remainder after division |
| **ring** | all hash values arranged on a circle |
| **virtual node (vnode)** | one of many points on the ring that belong to the same real server |
| **owner** | the server that stores a key |
| **remapped / moved key** | a key whose owner changed after a server was added or removed |
| **capacity** | maximum number of keys a server may hold (Rule 3) |
| **load** | number of keys a server currently holds |
| **linear probing** | if a slot is taken, try the next one, and so on |
| **open addressing** | a hash table design where collisions are solved by probing other slots |
| **`lower_bound`** | map function: first entry with key ≥ the given value |
| **balanced BST** | a binary search tree kept balanced so search is O(log n); `std::map` uses one |
| **seed** | starting value for the random number generator; same seed, same "random" numbers |
| **HTTP** | the text format browsers and web servers use to talk |
| **socket** | an operating system connection used to send and receive network data |
| **SVG** | a text format for pictures that browsers can display |
| **redirect (303)** | a server answer that tells the browser to load another address |
