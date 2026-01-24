#pragma once

#include <Arduino.h>
#include <vector>

struct BeszelSystem {
    String id;
    String name;
    String host;
    String status;
    float cpu;
    float mem;
    float disk;
    float net;
};

class BeszelService {
public:
    static BeszelService& getInstance();
    
    bool begin(const String& baseUrl);
    bool fetchSystems();
    
    const std::vector<BeszelSystem>& getSystems() const { return _systems; }
    size_t getSystemCount() const { return _systems.size(); }
    
private:
    BeszelService() : _baseUrl(""), _isInitialized(false) {}
    
    String _baseUrl;
    bool _isInitialized;
    std::vector<BeszelSystem> _systems;
};
