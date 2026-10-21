// text_utils.h
//
// Small helper functions for turning numbers into text, cleaning up text that
// goes into HTML, and decoding values that the browser sends.
//
// Nothing here is about hashing. These helpers just keep the other files
// shorter and easier to read.

#ifndef TEXT_UTILS_H
#define TEXT_UTILS_H

#include <string>

// Converts a whole number into text. Example: 1500 -> "1500"
std::string intToText(long long value);

// Converts a decimal number into text with a fixed number of digits after
// the dot. Example: decimalToText(3.14159, 2) -> "3.14"
std::string decimalToText(double value, int digitsAfterDot);

// Shows a size given in kilobytes in a friendly way.
// Example: 512 -> "512 KB", 2048 -> "2.00 MB"
std::string formatDataSize(long long kilobytes);

// Replaces characters that have a special meaning in HTML (< > & ")
// so user typed text can never break the page.
std::string escapeHtml(const std::string& text);

// Browsers encode form values before sending them ("node 1" becomes "node+1",
// "#" becomes "%23" and so on). This function reverses that.
std::string urlDecode(const std::string& text);

// Reads a number out of text. If the text is empty or not a number, the
// fallback is returned. The result is always clamped between minValue and
// maxValue so a user can not ask for, say, 10 billion keys.
int textToInt(const std::string& text, int fallback, int minValue, int maxValue);

#endif
