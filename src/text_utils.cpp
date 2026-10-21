// text_utils.cpp
//
// Implementation of the small text helpers declared in text_utils.h.
// std::ostringstream is used for number formatting because it works the same
// on every compiler (some older MinGW builds do not have std::to_string).

#include "text_utils.h"

#include <sstream>
#include <iomanip>
#include <cstdlib>

std::string intToText(long long value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

std::string decimalToText(double value, int digitsAfterDot)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(digitsAfterDot) << value;
    return out.str();
}

std::string formatDataSize(long long kilobytes)
{
    // Under 1 MB just print KB, otherwise switch to MB (and GB for
    // really large numbers) with two decimals.
    if (kilobytes < 1024)
    {
        return intToText(kilobytes) + " KB";
    }

    double megabytes = kilobytes / 1024.0;
    if (megabytes < 1024.0)
    {
        return decimalToText(megabytes, 2) + " MB";
    }

    double gigabytes = megabytes / 1024.0;
    return decimalToText(gigabytes, 2) + " GB";
}

std::string escapeHtml(const std::string& text)
{
    std::string result;

    for (size_t i = 0; i < text.size(); i++)
    {
        char c = text[i];

        if (c == '<')       result += "&lt;";
        else if (c == '>')  result += "&gt;";
        else if (c == '&')  result += "&amp;";
        else if (c == '"')  result += "&quot;";
        else                result += c;
    }

    return result;
}

// Helper for urlDecode: turns one hex digit ('0'-'9', 'a'-'f', 'A'-'F')
// into its value 0-15. Returns -1 if the character is not a hex digit.
static int hexDigitValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& text)
{
    std::string result;

    for (size_t i = 0; i < text.size(); i++)
    {
        char c = text[i];

        if (c == '+')
        {
            // In form data a '+' stands for a space
            result += ' ';
        }
        else if (c == '%' && i + 2 < text.size())
        {
            // "%41" means the byte with hex value 41, which is 'A'
            int high = hexDigitValue(text[i + 1]);
            int low = hexDigitValue(text[i + 2]);

            if (high >= 0 && low >= 0)
            {
                result += (char)(high * 16 + low);
                i += 2;
            }
            else
            {
                result += c;
            }
        }
        else
        {
            result += c;
        }
    }

    return result;
}

int textToInt(const std::string& text, int fallback, int minValue, int maxValue)
{
    if (text.empty())
    {
        return fallback;
    }

    // Make sure every character is a digit, otherwise use the fallback.
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return fallback;
        }
    }

    // Very long input would overflow an int, treat it as "too big".
    if (text.size() > 9)
    {
        return maxValue;
    }

    int value = std::atoi(text.c_str());

    if (value < minValue) value = minValue;
    if (value > maxValue) value = maxValue;

    return value;
}
