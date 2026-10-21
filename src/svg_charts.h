// svg_charts.h
//
// Draws charts as SVG text. SVG is just XML describing shapes
// (<rect>, <line>, <circle>, <text> ...), and every browser can show it
// directly inside an HTML page. That means the C++ server can produce the
// graphs itself, with no JavaScript and no charting library.

#ifndef SVG_CHARTS_H
#define SVG_CHARTS_H

#include <string>
#include <vector>
#include <map>

#include "consistent_hashing.h"

// One line or one set of bars in a chart.
struct ChartSeries
{
    std::string name;            // shown in the legend
    std::string color;           // e.g. "#4e79a7"
    std::vector<double> values;  // one value per x label
};

// Grouped bar chart: for each x label, one bar per series, side by side.
std::string drawBarChart(const std::string& title,
                         const std::vector<std::string>& xLabels,
                         const std::vector<ChartSeries>& series,
                         const std::string& yAxisLabel);

// Line chart: each series is a line going through its values.
std::string drawLineChart(const std::string& title,
                          const std::vector<std::string>& xLabels,
                          const std::vector<ChartSeries>& series,
                          const std::string& yAxisLabel);

// Draws the consistent hashing ring. Each coloured arc is the part of the
// ring a server is responsible for. `sampleKeys` are drawn as small dots
// inside the ring to show which arc they fall into.
std::string drawHashRing(const ConsistentHashRing& ring,
                         const std::map<std::string, std::string>& nodeColors,
                         const std::vector<std::string>& sampleKeys);

#endif
