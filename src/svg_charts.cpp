// svg_charts.cpp
//
// A tiny hand made charting "library". All charts share the same layout:
//
//     +--------------------------------------------------+
//     | title                              legend        |
//     |   y labels |  plot area (gridlines, bars/lines)  |
//     |            |                                     |
//     |            +-------------------------------------|
//     |                x labels                          |
//     +--------------------------------------------------+
//
// SVG coordinates start in the TOP-left corner and y grows DOWNWARDS, so a
// bigger value has to be drawn with a SMALLER y. valueToY() handles that.

#include "svg_charts.h"
#include "text_utils.h"
#include "hashing.h"

#include <sstream>
#include <cmath>

// ---------------------------------------------------------------------------
// Layout shared by the bar and line charts
// ---------------------------------------------------------------------------

struct ChartArea
{
    int width;        // whole picture
    int height;
    int left;         // where the plot area starts
    int top;
    int plotWidth;    // size of the plot area
    int plotHeight;
};

static ChartArea makeChartArea()
{
    ChartArea area;
    area.width = 680;
    area.height = 330;
    area.left = 66;
    area.top = 50;
    area.plotWidth = area.width - area.left - 18;
    area.plotHeight = area.height - area.top - 80;
    return area;
}

// Rounds the biggest value up to a "nice" number for the top of the y axis,
// e.g. 37 -> 50, 120 -> 200, 0.34 -> 0.5. Gridlines then land on round values.
static double roundUpToNiceNumber(double value)
{
    if (value <= 0)
    {
        return 1;
    }

    // Find the power of ten just below the value (1, 10, 100, ... or 0.1 ...)
    double magnitude = 1;
    while (magnitude * 10 <= value) magnitude = magnitude * 10;
    while (magnitude > value)       magnitude = magnitude / 10;

    if (value <= 1 * magnitude) return 1 * magnitude;
    if (value <= 2 * magnitude) return 2 * magnitude;
    if (value <= 5 * magnitude) return 5 * magnitude;
    return 10 * magnitude;
}

// Number text for axis ticks: no decimals for big numbers, some for small.
static std::string axisNumber(double value, double axisMax)
{
    if (axisMax >= 10)  return decimalToText(value, 0);
    if (axisMax >= 1)   return decimalToText(value, 1);
    return decimalToText(value, 2);
}

// Turns a value into a y pixel position inside the plot area.
static double valueToY(const ChartArea& area, double value, double axisMax)
{
    return area.top + area.plotHeight - (value / axisMax) * area.plotHeight;
}

// Finds the largest value over all series.
static double largestValue(const std::vector<ChartSeries>& series)
{
    double biggest = 0;
    for (size_t s = 0; s < series.size(); s++)
    {
        for (size_t i = 0; i < series[s].values.size(); i++)
        {
            if (series[s].values[i] > biggest)
            {
                biggest = series[s].values[i];
            }
        }
    }
    return biggest;
}

// Draws everything that is not data: title, gridlines, y axis numbers, the
// y axis name and the legend.
static void drawFrame(std::ostringstream& svg, const ChartArea& area,
                      const std::string& title, double axisMax,
                      const std::string& yAxisLabel,
                      const std::vector<ChartSeries>& series)
{
    svg << "<svg class=\"chart\" viewBox=\"0 0 " << area.width << " " << area.height
        << "\" xmlns=\"http://www.w3.org/2000/svg\" font-family=\"Arial, sans-serif\">\n";

    svg << "<rect x=\"0\" y=\"0\" width=\"" << area.width << "\" height=\"" << area.height
        << "\" fill=\"#ffffff\"/>\n";

    // Title
    svg << "<text x=\"" << area.left << "\" y=\"22\" font-size=\"15\" font-weight=\"600\" fill=\"#222\">"
        << escapeHtml(title) << "</text>\n";

    // Five horizontal gridlines with their numbers
    const int gridSteps = 5;
    for (int step = 0; step <= gridSteps; step++)
    {
        double value = axisMax * step / gridSteps;
        double y = valueToY(area, value, axisMax);

        svg << "<line x1=\"" << area.left << "\" y1=\"" << y
            << "\" x2=\"" << area.left + area.plotWidth << "\" y2=\"" << y
            << "\" stroke=\"" << (step == 0 ? "#888" : "#e4e4e4") << "\" stroke-width=\"1\"/>\n";

        svg << "<text x=\"" << area.left - 8 << "\" y=\"" << y + 4
            << "\" font-size=\"11\" fill=\"#555\" text-anchor=\"end\">"
            << axisNumber(value, axisMax) << "</text>\n";
    }

    // Name of the y axis, written sideways
    int middleY = area.top + area.plotHeight / 2;
    svg << "<text x=\"14\" y=\"" << middleY << "\" font-size=\"11\" fill=\"#555\" text-anchor=\"middle\""
        << " transform=\"rotate(-90 14 " << middleY << ")\">" << escapeHtml(yAxisLabel) << "</text>\n";

    // Legend in one row under the title, from left to right. The text width
    // is guessed as ~6px per letter, good enough for short names.
    int legendX = area.left;
    for (size_t s = 0; s < series.size(); s++)
    {
        svg << "<rect x=\"" << legendX << "\" y=\"30\" width=\"11\" height=\"11\" fill=\""
            << series[s].color << "\"/>\n";
        svg << "<text x=\"" << legendX + 15 << "\" y=\"40\" font-size=\"11\" fill=\"#333\">"
            << escapeHtml(series[s].name) << "</text>\n";

        legendX = legendX + 15 + (int)series[s].name.size() * 6 + 18;
    }
}

// Writes the x axis labels. When there are many labels only every few of
// them are shown, and tilt them so they don't overlap.
static void drawXLabels(std::ostringstream& svg, const ChartArea& area,
                        const std::vector<std::string>& xLabels,
                        const std::vector<double>& xPositions)
{
    int count = (int)xLabels.size();
    int every = 1;
    if (count > 16)
    {
        every = (count + 15) / 16;
    }

    bool tilt = count > 7;
    double y = area.top + area.plotHeight + 16;

    for (int i = 0; i < count; i += every)
    {
        double x = xPositions[i];

        if (tilt)
        {
            svg << "<text x=\"" << x << "\" y=\"" << y << "\" font-size=\"10\" fill=\"#444\""
                << " text-anchor=\"end\" transform=\"rotate(-35 " << x << " " << y << ")\">"
                << escapeHtml(xLabels[i]) << "</text>\n";
        }
        else
        {
            svg << "<text x=\"" << x << "\" y=\"" << y << "\" font-size=\"11\" fill=\"#444\""
                << " text-anchor=\"middle\">" << escapeHtml(xLabels[i]) << "</text>\n";
        }
    }
}

// Used when a chart has nothing to show yet.
static void drawNoDataNote(std::ostringstream& svg, const ChartArea& area)
{
    svg << "<text x=\"" << area.left + area.plotWidth / 2 << "\" y=\"" << area.top + area.plotHeight / 2
        << "\" font-size=\"13\" fill=\"#999\" text-anchor=\"middle\">nothing to show yet</text>\n";
}

// ---------------------------------------------------------------------------
// Bar chart
// ---------------------------------------------------------------------------

std::string drawBarChart(const std::string& title,
                         const std::vector<std::string>& xLabels,
                         const std::vector<ChartSeries>& series,
                         const std::string& yAxisLabel)
{
    ChartArea area = makeChartArea();
    double axisMax = roundUpToNiceNumber(largestValue(series));

    std::ostringstream svg;
    drawFrame(svg, area, title, axisMax, yAxisLabel, series);

    if (xLabels.empty() || series.empty())
    {
        drawNoDataNote(svg, area);
        svg << "</svg>\n";
        return svg.str();
    }

    // Each x label gets an equal slice ("group") of the width. Inside the
    // group 10% is left empty on each side and the rest is split between the
    // series.
    double groupWidth = (double)area.plotWidth / xLabels.size();
    double barWidth = groupWidth * 0.8 / series.size();

    std::vector<double> labelPositions;

    for (size_t i = 0; i < xLabels.size(); i++)
    {
        double groupStart = area.left + i * groupWidth;
        labelPositions.push_back(groupStart + groupWidth / 2);

        for (size_t s = 0; s < series.size(); s++)
        {
            double value = 0;
            if (i < series[s].values.size())
            {
                value = series[s].values[i];
            }

            double x = groupStart + groupWidth * 0.1 + s * barWidth;
            double y = valueToY(area, value, axisMax);
            double height = area.top + area.plotHeight - y;

            // <title> inside a shape = the browser shows it as a tooltip on hover
            svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << barWidth * 0.92
                << "\" height=\"" << height << "\" fill=\"" << series[s].color << "\">"
                << "<title>" << escapeHtml(xLabels[i]) << " / " << escapeHtml(series[s].name)
                << ": " << decimalToText(value, 2) << "</title></rect>\n";
        }
    }

    drawXLabels(svg, area, xLabels, labelPositions);
    svg << "</svg>\n";
    return svg.str();
}

// ---------------------------------------------------------------------------
// Line chart
// ---------------------------------------------------------------------------

std::string drawLineChart(const std::string& title,
                          const std::vector<std::string>& xLabels,
                          const std::vector<ChartSeries>& series,
                          const std::string& yAxisLabel)
{
    ChartArea area = makeChartArea();
    double axisMax = roundUpToNiceNumber(largestValue(series));

    std::ostringstream svg;
    drawFrame(svg, area, title, axisMax, yAxisLabel, series);

    if (xLabels.empty() || series.empty())
    {
        drawNoDataNote(svg, area);
        svg << "</svg>\n";
        return svg.str();
    }

    // Spread the points evenly, keeping a small gap at both ends.
    const double inset = 16;
    std::vector<double> xPositions;
    for (size_t i = 0; i < xLabels.size(); i++)
    {
        double x;
        if (xLabels.size() == 1)
        {
            x = area.left + area.plotWidth / 2.0;
        }
        else
        {
            x = area.left + inset + i * (area.plotWidth - 2 * inset) / (xLabels.size() - 1);
        }
        xPositions.push_back(x);
    }

    bool showDots = xLabels.size() <= 40;

    for (size_t s = 0; s < series.size(); s++)
    {
        // A polyline is one shape through a list of "x,y" points
        svg << "<polyline fill=\"none\" stroke=\"" << series[s].color
            << "\" stroke-width=\"2.2\" points=\"";
        for (size_t i = 0; i < series[s].values.size() && i < xPositions.size(); i++)
        {
            svg << xPositions[i] << "," << valueToY(area, series[s].values[i], axisMax) << " ";
        }
        svg << "\"/>\n";

        if (showDots)
        {
            for (size_t i = 0; i < series[s].values.size() && i < xPositions.size(); i++)
            {
                double value = series[s].values[i];
                svg << "<circle cx=\"" << xPositions[i] << "\" cy=\"" << valueToY(area, value, axisMax)
                    << "\" r=\"3.2\" fill=\"" << series[s].color << "\">"
                    << "<title>" << escapeHtml(xLabels[i]) << " / " << escapeHtml(series[s].name)
                    << ": " << decimalToText(value, 2) << "</title></circle>\n";
            }
        }
    }

    drawXLabels(svg, area, xLabels, xPositions);
    svg << "</svg>\n";
    return svg.str();
}

// ---------------------------------------------------------------------------
// Hash ring
// ---------------------------------------------------------------------------

// Ring position (0 .. 2^32-1) -> angle in radians. 90 degrees are subtracted so
// position 0 is at the top (12 o'clock) and values grow clockwise.
static double positionToAngle(unsigned int position)
{
    const double PI = 3.14159265358979;
    return position / 4294967296.0 * 2 * PI - PI / 2;
}

// Writes an SVG arc from angle a1 to angle a2 (clockwise) on a circle.
static void drawArc(std::ostringstream& svg, double centerX, double centerY, double radius,
                    double startAngle, double endAngle, const std::string& color,
                    const std::string& tooltip)
{
    const double PI = 3.14159265358979;

    // A full lap would start and end on the same point, and SVG draws
    // nothing for that. It happens when there is only one server, so a
    // plain circle is drawn instead.
    if (endAngle - startAngle >= 2 * PI - 0.000001)
    {
        svg << "<circle cx=\"" << centerX << "\" cy=\"" << centerY << "\" r=\"" << radius
            << "\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"22\">"
            << "<title>" << escapeHtml(tooltip) << "</title></circle>\n";
        return;
    }

    double x1 = centerX + radius * std::cos(startAngle);
    double y1 = centerY + radius * std::sin(startAngle);
    double x2 = centerX + radius * std::cos(endAngle);
    double y2 = centerY + radius * std::sin(endAngle);

    // SVG needs to know if the arc is the long way (> 180 degrees) around
    int largeArc = (endAngle - startAngle) > PI ? 1 : 0;

    svg << "<path d=\"M " << x1 << " " << y1 << " A " << radius << " " << radius
        << " 0 " << largeArc << " 1 " << x2 << " " << y2
        << "\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"22\">"
        << "<title>" << escapeHtml(tooltip) << "</title></path>\n";
}

std::string drawHashRing(const ConsistentHashRing& ring,
                         const std::map<std::string, std::string>& nodeColors,
                         const std::vector<std::string>& sampleKeys)
{
    const double PI = 3.14159265358979;
    const int size = 380;
    const double centerX = size / 2.0;
    const double centerY = size / 2.0;
    const double radius = 150;

    std::ostringstream svg;
    svg << "<svg class=\"ring\" viewBox=\"0 0 " << size << " " << size
        << "\" xmlns=\"http://www.w3.org/2000/svg\" font-family=\"Arial, sans-serif\">\n";

    const std::map<unsigned int, std::string>& points = ring.getRingPoints();

    if (points.empty())
    {
        svg << "<text x=\"" << centerX << "\" y=\"" << centerY << "\" text-anchor=\"middle\">empty ring</text></svg>";
        return svg.str();
    }

    // Look up a colour, grey if somehow missing
    std::map<std::string, std::string>::const_iterator colorIt;

    if (points.size() == 1)
    {
        // One vnode owns the whole circle
        colorIt = nodeColors.find(points.begin()->second);
        std::string color = colorIt != nodeColors.end() ? colorIt->second : "#999";
        svg << "<circle cx=\"" << centerX << "\" cy=\"" << centerY << "\" r=\"" << radius
            << "\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"22\"/>\n";
    }
    else
    {
        // Walk around the ring. Every vnode owns the arc that ENDS at it.
        // Neighbouring vnodes of the same server are merged into one longer
        // arc, which keeps the SVG small when there are thousands of vnodes.
        std::map<unsigned int, std::string>::const_iterator it = points.begin();

        double runStart = positionToAngle(points.rbegin()->first) - 2 * PI;  // last vnode, one lap back
        double runEnd = positionToAngle(it->first);
        std::string runOwner = it->second;
        ++it;

        for (; it != points.end(); ++it)
        {
            double angle = positionToAngle(it->first);

            if (it->second == runOwner)
            {
                runEnd = angle;   // same server, just make the arc longer
            }
            else
            {
                colorIt = nodeColors.find(runOwner);
                drawArc(svg, centerX, centerY, radius, runStart, runEnd,
                        colorIt != nodeColors.end() ? colorIt->second : "#999", runOwner);

                runStart = runEnd;
                runEnd = angle;
                runOwner = it->second;
            }
        }

        colorIt = nodeColors.find(runOwner);
        drawArc(svg, centerX, centerY, radius, runStart, runEnd,
                colorIt != nodeColors.end() ? colorIt->second : "#999", runOwner);
    }

    // Small white ticks where each vnode sits (skipped if there are so many
    // that they would just make a white blur)
    if (points.size() <= 400)
    {
        std::map<unsigned int, std::string>::const_iterator it;
        for (it = points.begin(); it != points.end(); ++it)
        {
            double angle = positionToAngle(it->first);
            double xIn = centerX + (radius - 11) * std::cos(angle);
            double yIn = centerY + (radius - 11) * std::sin(angle);
            double xOut = centerX + (radius + 11) * std::cos(angle);
            double yOut = centerY + (radius + 11) * std::sin(angle);
            svg << "<line x1=\"" << xIn << "\" y1=\"" << yIn << "\" x2=\"" << xOut << "\" y2=\"" << yOut
                << "\" stroke=\"#fff\" stroke-width=\"1.2\"/>\n";
        }
    }

    // Sample keys: dark dots just inside the ring, hover shows the owner
    for (size_t i = 0; i < sampleKeys.size(); i++)
    {
        unsigned int position = hashString(sampleKeys[i]);
        double angle = positionToAngle(position);
        double x = centerX + (radius - 22) * std::cos(angle);
        double y = centerY + (radius - 22) * std::sin(angle);

        svg << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"3\" fill=\"#333\">"
            << "<title>" << escapeHtml(sampleKeys[i]) << " -> " << escapeHtml(ring.getNodeForKey(sampleKeys[i]))
            << "</title></circle>\n";
    }

    // Text in the middle
    svg << "<text x=\"" << centerX << "\" y=\"" << centerY - 6 << "\" text-anchor=\"middle\" font-size=\"14\" fill=\"#333\">"
        << intToText((long long)points.size()) << " ring points</text>\n";
    svg << "<text x=\"" << centerX << "\" y=\"" << centerY + 14 << "\" text-anchor=\"middle\" font-size=\"11\" fill=\"#777\">"
        << "0 at the top, clockwise</text>\n";

    svg << "</svg>\n";
    return svg.str();
}
