#include "html_utils.h"

String html_decode_entities(const String& str) {
    String result = str;
    
    // Common HTML entities
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&apos;", "'");
    result.replace("&#39;", "'");
    result.replace("&nbsp;", " ");
    
    // Typographical quotes and dashes
    result.replace("&ndash;", "-");
    result.replace("&mdash;", "-");
    result.replace("&hellip;", "...");
    result.replace("&rsquo;", "'");
    result.replace("&lsquo;", "'");
    result.replace("&rdquo;", "\"");
    result.replace("&ldquo;", "\"");
    
    // Remove CDATA tags if present
    result.replace("<![CDATA[", "");
    result.replace("]]>", "");
    
    return result;
}

String html_strip_tags(const String& html) {
    String result = "";
    result.reserve(html.length());
    
    bool inTag = false;
    bool inScript = false;
    bool inStyle = false;
    
    for (int i = 0; i < html.length(); i++) {
        char c = html.charAt(i);
        
        // Check for script/style tags
        if (c == '<' && i + 7 < html.length()) {
            String tag = html.substring(i, i + 8);
            tag.toLowerCase();
            if (tag.startsWith("<script")) inScript = true;
            if (tag.startsWith("<style")) inStyle = true;
        }
        if (c == '<' && i + 8 < html.length()) {
            String tag = html.substring(i, i + 9);
            tag.toLowerCase();
            if (tag == "</script>") inScript = false;
            if (tag == "</style>") inStyle = false;
        }
        
        if (c == '<') {
            inTag = true;
        } else if (c == '>') {
            inTag = false;
        } else if (!inTag && !inScript && !inStyle) {
             result += c;
        }
    }

    // Decode entities in the stripped text
    return html_decode_entities(result);
}

void html_strip_tags_inplace(char* buffer, size_t length) {
    if (!buffer || length == 0) return;
    
    char* read = buffer;
    char* write = buffer;
    char* end = buffer + length;
    
    bool inTag = false;
    bool inScript = false;
    bool inStyle = false;
    
    while (read < end) {
        char c = *read;
        
        // Check for script/style tags start (case-insensitive comparison)
        if (c == '<') {
            if (read + 7 < end) {
                // Check for <script (case insensitive)
                if ((read[1] == 's' || read[1] == 'S') &&
                    (read[2] == 'c' || read[2] == 'C') &&
                    (read[3] == 'r' || read[3] == 'R') &&
                    (read[4] == 'i' || read[4] == 'I') &&
                    (read[5] == 'p' || read[5] == 'P') &&
                    (read[6] == 't' || read[6] == 'T')) {
                    inScript = true;
                }
            }
            if (read + 6 < end) {
                // Check for <style (case insensitive)
                if ((read[1] == 's' || read[1] == 'S') &&
                    (read[2] == 't' || read[2] == 'T') &&
                    (read[3] == 'y' || read[3] == 'Y') &&
                    (read[4] == 'l' || read[4] == 'L') &&
                    (read[5] == 'e' || read[5] == 'E')) {
                    inStyle = true;
                }
            }
        }
        
        // Check for script/style tags end
        if (c == '<' && (inScript || inStyle)) {
            if (read + 8 < end) {
                // Check for </script>
                if (inScript && read[1] == '/' &&
                    (read[2] == 's' || read[2] == 'S') &&
                    (read[3] == 'c' || read[3] == 'C') &&
                    (read[4] == 'r' || read[4] == 'R') &&
                    (read[5] == 'i' || read[5] == 'I') &&
                    (read[6] == 'p' || read[6] == 'P') &&
                    (read[7] == 't' || read[7] == 'T') &&
                    read[8] == '>') {
                    inScript = false;
                }
                // Check for </style>
                else if (inStyle && read[1] == '/' &&
                    (read[2] == 's' || read[2] == 'S') &&
                    (read[3] == 't' || read[3] == 'T') &&
                    (read[4] == 'y' || read[4] == 'Y') &&
                    (read[5] == 'l' || read[5] == 'L') &&
                    (read[6] == 'e' || read[6] == 'E') &&
                    read[7] == '>') {
                    inStyle = false;
                }
            }
        }

        if (c == '<') {
            inTag = true;
        } else if (c == '>') {
            inTag = false;
            read++;
            continue;
        }

        if (!inTag && !inScript && !inStyle) {
            *write++ = c;
        }
        
        read++;
    }
    
    *write = 0; // Null terminate
    
    // Second pass for entities (in-place decode)
    size_t newLen = write - buffer;
    read = buffer;
    write = buffer;
    end = buffer + newLen;

    while (read < end) {
        if (*read == '&') {
            // Check common entities
            if (read + 3 < end && strncmp(read, "&lt;", 4) == 0) { *write++ = '<'; read += 4; continue; }
            if (read + 3 < end && strncmp(read, "&gt;", 4) == 0) { *write++ = '>'; read += 4; continue; }
            if (read + 4 < end && strncmp(read, "&amp;", 5) == 0) { *write++ = '&'; read += 5; continue; }
            if (read + 5 < end && strncmp(read, "&quot;", 6) == 0) { *write++ = '"'; read += 6; continue; }
            if (read + 5 < end && strncmp(read, "&apos;", 6) == 0) { *write++ = '\''; read += 6; continue; }
            if (read + 5 < end && strncmp(read, "&nbsp;", 6) == 0) { *write++ = ' '; read += 6; continue; }
            // Unknown entity, just copy
            *write++ = *read++;
        } else {
            *write++ = *read++;
        }
    }
    *write = 0;
}
