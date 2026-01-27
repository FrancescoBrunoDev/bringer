#include "zip_utils.h"
#include <uzlib/uzlib.h>
#include "logger/logger.h"

// Define a safe buffer size for scanning
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
        logger_log("ZipReader: EOCD not found in %s", path.c_str());
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
    
    free(scanBuf);
    return found;
}

std::vector<String> ZipReader::listFiles(const String& extensionSuffix) {
    std::vector<String> result;
    if (!_isOpen) return result;

    _f.seek(_cdOffset);

    for (int i = 0; i < _totalEntries; i++) {
        if (_f.position() >= _f.size()) break;

        // CD File Header Signature
        uint8_t sig[4];
        if (_f.read(sig, 4) != 4) break;
        if (!(sig[0] == 0x50 && sig[1] == 0x4B && sig[2] == 0x01 && sig[3] == 0x02)) break;

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
        
        String filename = "";
        for(int k=0; k<nameLen; k++) filename += (char)_f.read();
        
        bool match = true;
        if (extensionSuffix.length() > 0) {
            match = filename.endsWith(extensionSuffix);
            // Handle cases like .html vs .xhtml? Caller handles specific or we iterate.
            // For now, simple suffix match.
        }

        if (match) {
            result.push_back(filename);
        }

        // Skip extra and comment
        _f.seek(_f.position() + extraLen + commentLen);

        yield(); // Feed watchdog
    }

    return result;
}

bool ZipReader::readFile(const String& filename, String& outContent) {
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
    if (uncompSize > 64 * 1024) uncompSize = 64 * 1024; // Limit output size
    if (compSize > 128 * 1024) return false; // Limit compressed input

    if (method == 0) {
        // STORED
        if (compSize > 0) {
            uint8_t* raw = (uint8_t*)malloc(compSize + 1);
            if (raw) {
                _f.read(raw, compSize);
                raw[compSize] = 0;
                outContent = (char*)raw;
                free(raw);
                return true;
            }
        }
    } else if (method == 8) {
        // DEFLATE
        uint8_t* compData = (uint8_t*)malloc(compSize);
        if(!compData) return false;
        
        _f.read(compData, compSize);
        
        uint8_t* uncompData = (uint8_t*)malloc(uncompSize + 1);
        if(!uncompData) { 
            free(compData); 
            return false; 
        }
        
        // Decompress
        TINF_DATA d;
        uzlib_init();
        d.source = compData;
        d.source_limit = compData + compSize;
        d.destStart = uncompData;
        d.dest = uncompData;
        
        uzlib_uncompress_init(&d, NULL, 0);
        int res = uzlib_uncompress(&d);
        
        if (res == TINF_DONE) {
            size_t actualSize = d.dest - uncompData;
            uncompData[actualSize] = 0;
            outContent = (char*)uncompData;
            free(uncompData);
            free(compData);
            return true;
        }
        
        free(uncompData);
        free(compData);
    }

    return false;
}
