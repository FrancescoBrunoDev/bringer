#include "zip_utils.h"
#include "logger/logger.h"
#include <uzlib/uzlib.h>

// Define a safe buffer size for scanning (increased for better EOCD search)
#define SCAN_BUF_SIZE 4096

ZipReader::ZipReader() : _cdOffset(0), _totalEntries(0), _isOpen(false) {}

ZipReader::~ZipReader() {
    close();
}

void ZipReader::close() {
    if (_f) _f.close();
    _isOpen = false;
}

bool ZipReader::open(const String& path) {
    if (_isOpen) close();

    _f = LittleFS.open(path, "r");
    if (!_f) {
        logger_log("ZipReader: Failed to open %s", path.c_str());
        return false;
    }

    uint32_t eocdPos = 0;
    if (!findEOCD(eocdPos)) {
        logger_log("ZipReader: EOCD not found in %s (Size: %d)", path.c_str(), _f.size());
        close();
        return false;
    }

    // Read CD info from EOCD
    // Offset 10: Total Entries (2 bytes)
    // Offset 16: CD Offset (4 bytes)
    _f.seek(eocdPos + 10);
    _totalEntries = _f.read() | (_f.read() << 8);
    
    _f.seek(eocdPos + 16);
    _cdOffset = _f.read() | (_f.read() << 8) | (_f.read() << 16) | (_f.read() << 24);

    _isOpen = true;
    return true;
}

bool ZipReader::findEOCD(uint32_t& outEocdPos) {
    size_t fileSize = _f.size();
    if (fileSize < 22) return false;

    size_t scanSize = (fileSize > SCAN_BUF_SIZE) ? SCAN_BUF_SIZE : fileSize;
    size_t scanStart = fileSize - scanSize;

    _f.seek(scanStart);
    uint8_t* scanBuf = (uint8_t*)malloc(scanSize);
    if (!scanBuf) return false;

    _f.read(scanBuf, scanSize);

    bool found = false;
    // Scan backwards
    for (int i = scanSize - 4; i >= 0; i--) {
        if (scanBuf[i] == 0x50 && scanBuf[i+1] == 0x4B && scanBuf[i+2] == 0x05 && scanBuf[i+3] == 0x06) {
            outEocdPos = scanStart + i;
            found = true;
            break;
        }
    }
    
    if (!found) {
        // Debug: Print last few bytes to see what's there
        String hex = "";
        int debugCount = (scanSize > 16) ? 16 : scanSize;
        for(int i = scanSize - debugCount; i < scanSize; i++) {
             char tmp[4];
             snprintf(tmp, sizeof(tmp), "%02X ", scanBuf[i]);
             hex += tmp;
        }
        logger_log("ZipReader: Tail bytes: %s", hex.c_str());
    }

    free(scanBuf);
    return found;
}

void ZipReader::processFileEntries(std::function<bool(const String& name)> callback) {
    if (!_isOpen || !callback) return;
    
    logger_log("ZipReader: CD Offset: %u, Total Entries: %u", _cdOffset, _totalEntries);
    
    // Sanity check
    if (_totalEntries > 10000) {
         logger_log("ZipReader: Aborting, too many entries");
         return;
    }

    _f.seek(_cdOffset);

    for (int i = 0; i < _totalEntries; i++) {
        if (_f.position() >= _f.size()) break;

        // CD File Header Signature
        uint8_t sig[4];
        if (_f.read(sig, 4) != 4) {
            logger_log("ZipReader: Short read on sig at entry %d", i);
            break;
        }
        if (!(sig[0] == 0x50 && sig[1] == 0x4B && sig[2] == 0x01 && sig[3] == 0x02)) {
             logger_log("ZipReader: Bad sig at entry %d: %02X %02X %02X %02X", i, sig[0], sig[1], sig[2], sig[3]);
             break;
        }

        // Offset 28: Filename Length (2 bytes)
        // Offset 30: Extra Field Length (2 bytes)
        // Offset 32: Comment Length (2 bytes)
        // Offset 46: Filename (variable)
        
        _f.seek(_f.position() + 24); // Skip to lengths
        uint16_t nameLen = _f.read() | (_f.read() << 8);
        uint16_t extraLen = _f.read() | (_f.read() << 8);
        uint16_t commentLen = _f.read() | (_f.read() << 8);
        
        // Skip remaining header (12 bytes from 34 to 46)
        _f.seek(_f.position() + 12);
        
        // Safety: Cap nameLen to prevent massive or infinite reads
        if (nameLen > 512) {
             logger_log("ZipReader: Filename too long/corrupt (%d) at entry %d, skipping", nameLen, i);
             // Skip this entry and continue
             _f.seek(_f.position() + nameLen + extraLen + commentLen);
             continue; 
        }

        String filename = "";
        char nameBuf[513]; // +1 for null
        
        if (nameLen > 0) {
            size_t readLen = _f.read((uint8_t*)nameBuf, nameLen);
            if (readLen != nameLen) {
                 logger_log("ZipReader: Error reading filename at entry %d", i);
                 break;
            }
            nameBuf[readLen] = 0;
            filename = String(nameBuf);
        }
        
        if (!callback(filename)) {
             // Stop requested
             break;
        }

        // Skip extra and comment
        _f.seek(_f.position() + extraLen + commentLen);

        delay(1); // Feed watchdog
    }
    logger_log("ZipReader: Scan complete");
}

std::vector<String> ZipReader::listFiles(const String& extensionSuffix) {
    std::vector<String> result;
    processFileEntries([&](const String& filename) -> bool {
        bool match = true;
        if (extensionSuffix.length() > 0) {
            match = filename.endsWith(extensionSuffix);
        }

        if (match) {
            result.push_back(filename);
        }
        return true;
    });
    return result;
}

bool ZipReader::readBinary(const String& filename, uint8_t** outBuf, size_t* outSize) {
    if (!_isOpen) return false;

    _f.seek(_cdOffset);

    bool entryFound = false;
    uint16_t method = 0;
    uint32_t compSize = 0;
    uint32_t uncompSize = 0;
    uint32_t localHeaderOffset = 0;

    for (int i = 0; i < _totalEntries; i++) {
        if (_f.position() >= _f.size()) break;

        // CD Signature
        long sigStart = _f.position();
        uint8_t sig[4];
        if (_f.read(sig, 4) != 4) break;
        if (!(sig[0] == 0x50 && sig[1] == 0x4B && sig[2] == 0x01 && sig[3] == 0x02)) break;
        
        // Offset 10: Method
        _f.seek(sigStart + 10);
        method = _f.read() | (_f.read() << 8);

        // Offset 20: Compressed Size
        _f.seek(sigStart + 20);
        compSize = _f.read() | (_f.read() << 8) | (_f.read() << 16) | (_f.read() << 24);
        uncompSize = _f.read() | (_f.read() << 8) | (_f.read() << 16) | (_f.read() << 24);

        // Offset 28: Lengths
        uint16_t nameLen = _f.read() | (_f.read() << 8);
        uint16_t extraLen = _f.read() | (_f.read() << 8);
        uint16_t commentLen = _f.read() | (_f.read() << 8);

        // Offset 42: Local Header Offset
        _f.seek(sigStart + 42);
        localHeaderOffset = _f.read() | (_f.read() << 8) | (_f.read() << 16) | (_f.read() << 24);

        // Offset 46: Filename
        String fname = "";
        for(int k=0; k<nameLen; k++) fname += (char)_f.read();

        // Advance specific amount to next entry start
        long nextEntryPos = sigStart + 46 + nameLen + extraLen + commentLen;

        if (fname == filename) {
            entryFound = true;
            break; 
        }

        _f.seek(nextEntryPos);
    }

    if (!entryFound) return false;

    // Go to Local Header
    _f.seek(localHeaderOffset);
    // Skip 26 bytes to file name length
    _f.seek(_f.position() + 26);
    uint16_t n = _f.read() | (_f.read() << 8);
    uint16_t m = _f.read() | (_f.read() << 8);
    
    // Skip name + extra
    _f.seek(_f.position() + n + m);

    // Read Data
    // Protection against massive files
    // If we simply truncate the allocation size but the decompressor writes more, we corrupt memory.
    // So we must REJECT files that are too big for RAM.
    const size_t MAX_UNCOMP_SIZE = 120 * 1024; // 120KB limit (leave room for OS)
    if (uncompSize > MAX_UNCOMP_SIZE) {
        logger_log("ZipReader: Entry too large (%d > %d)", uncompSize, MAX_UNCOMP_SIZE);
        return false;
    }
    if (compSize > 128 * 1024) return false; // Limit compressed input

    logger_log("ZipReader: Entry method=%d, compSize=%d, uncompSize=%d", method, compSize, uncompSize);

    if (method == 0) {
        // STORED
        logger_log("ZipReader: Reading STORED entry");
        if (compSize > 0) {
            uint8_t* raw = (uint8_t*)malloc(compSize + 1);
            if (raw) {
                _f.read(raw, compSize);
                raw[compSize] = 0;
                *outBuf = raw;
                *outSize = compSize;
                logger_log("ZipReader: STORED read complete");
                return true;
            }
        }
    } else if (method == 8) {
        // DEFLATE
        logger_log("ZipReader: Decompressing DEFLATE entry");
        
        #ifdef CONFIG_IDF_TARGET_ESP32C6
        // uzlib is not compatible with ESP32-C6 (RISC-V architecture)
        logger_log("ZipReader: ERROR - Compressed EPUBs not supported on ESP32-C6");
        logger_log("ZipReader: Please use uncompressed EPUB files or ESP32-S3 board");
        return false;
        #else
        
        uint8_t* compData = (uint8_t*)malloc(compSize);
        if(!compData) {
            logger_log("ZipReader: Failed to allocate compData");
            return false;
        }
        
        logger_log("ZipReader: Reading %d compressed bytes", compSize);
        _f.read(compData, compSize);
        
        logger_log("ZipReader: Allocating %d bytes for uncompressed", uncompSize);
        uint8_t* uncompData = (uint8_t*)malloc(uncompSize + 1);
        if(!uncompData) { 
            logger_log("ZipReader: Failed to allocate uncompData");
            free(compData); 
            return false; 
        }
        
        // Decompress using uz lib (Xtensa only - ESP32/S2/S3)
        logger_log("ZipReader: Calling uzlib decompress...");
        TINF_DATA d;
        d.source = compData;
        d.source_limit = compData + compSize;
        d.destStart = uncompData;
        d.dest = uncompData;
        
        uzlib_uncompress_init(&d, NULL, 0);
        int res = uzlib_uncompress(&d);
        
        free(compData); // Free compressed data immediately after use
        
        logger_log("ZipReader: uzlib returned %d", res);

        if (res == TINF_DONE) {
            size_t actualSize = d.dest - uncompData;
            uncompData[actualSize] = 0;
            *outBuf = uncompData;
            *outSize = actualSize;
            logger_log("ZipReader: Decompression successful");
            return true;
        }
        
        logger_log("ZipReader: Decompression failed");
        free(uncompData);
        #endif
    }

    return false;
}

bool ZipReader::readFile(const String& filename, String& outContent) {
    uint8_t* buf = NULL;
    size_t size = 0;
    if (readBinary(filename, &buf, &size)) {
        if (buf) {
            // This still does a copy, which is what we want to avoid in optimized paths
            // but for backward compatibility it handles it.
            // Using String(char*) logic. since we null terminated it in readBinary, it serves as c-string.
            // However, if binary data contains nulls, String constructor might stop early.
            // But this method is 'readFile' mostly used for text in this codebase context.
            outContent = String((char*)buf);
            free(buf);
            return true;
        }
    }
    return false;
}
