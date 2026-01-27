#pragma once

#include <Arduino.h>

/**
 * @brief Decodes HTML entities in a string.
 * Supports common entities like &amp;, &lt;, &gt;, &quot;, &apos;, and &nbsp;.
 * 
 * @param str Input string with HTML entities.
 * @return String Decoded string.
 */
String html_decode_entities(const String& str);

/**
 * @brief Strips all HTML tags from a string and decodes entities.
 * 
 * @param html Input HTML string associated with the content.
 * @return String Plain text with tags removed and entities decoded.
 */
String html_strip_tags(const String& html);
