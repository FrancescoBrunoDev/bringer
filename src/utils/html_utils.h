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

/**
 * @brief Strips HTML tags from a buffer in-place.
 * Also decodes entities. The result will always be shorter or equal length.
 * The buffer will be null-terminated at the new length.
 * 
 * @param buffer Mutable buffer containing HTML.
 * @param length Length of the valid data in buffer.
 */
void html_strip_tags_inplace(char* buffer, size_t length);
