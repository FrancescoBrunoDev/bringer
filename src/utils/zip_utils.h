#pragma once

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>

class ZipReader {
public:
    ZipReader();
    ~ZipReader();

    /**
     * @brief Opens a ZIP file for reading.
     * 
     * @param path Absolute path to the ZIP file in LittleFS.
     * @return true If opened and validated (EOCD found).
     * @return false If failed.
     */
    bool open(const String& path);

    /**
     * @brief Closes the file.
     */
    void close();

    /**
     * @brief Lists files in the ZIP, optionally filtering by extension.
     * 
     * @param extensionSuffix Filter, e.g., ".html". Empty returns all.
     * @return std::vector<String> List of filenames found.
     */
    std::vector<String> listFiles(const String& extensionSuffix = "");

    /**
     * @brief Reads the content of a specific file within the ZIP.
     * Decompresses if necessary (DEFLATE).
     * 
     * @param filename Name of the file to read (must match exactly).
     * @param outContent String to store the result.
     * @return true If read successfully.
     * @return false If not found or decompression failed.
     */
    bool readFile(const String& filename, String& outContent);

private:
    File _f;
    uint32_t _cdOffset;
    uint16_t _totalEntries;
    bool _isOpen;

    bool findEOCD(uint32_t& outEocdPos);
};
