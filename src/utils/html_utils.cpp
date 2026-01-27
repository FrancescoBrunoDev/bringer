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
