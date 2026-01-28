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
     * @brief Iterates through all files in the ZIP and calls the callback for each.
     * Use this to avoid allocating a vector of all filenames when you only need a few.
     * 
     * @param callback Function to call for each file. Return true to continue, false to stop.
     */
    void processFileEntries(std::function<bool(const String& name)> callback);

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

    /**
     * @brief Reads the content of a specific file within the ZIP into a raw buffer.
     * Use this for large files to avoid String overhead.
     * The caller is responsible for freeing outBuf using free().
     * 
     * @param filename Name of the file to read.
     * @param outBuf Pointer to the buffer pointer. Will be allocated if successful.
     * @param outSize Pointer to size_t to store the size of the read data.
     * @return true If read successfully.
     */
    bool readBinary(const String& filename, uint8_t** outBuf, size_t* outSize);

private:
    File _f;
    uint32_t _cdOffset;
    uint16_t _totalEntries;
    bool _isOpen;

    bool findEOCD(uint32_t& outEocdPos);
};
