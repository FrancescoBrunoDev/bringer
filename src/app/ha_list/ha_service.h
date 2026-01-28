#pragma once
#include <Arduino.h>
#include <vector>

struct HAShoppingItem {
    String id;
    String name;
    bool complete;
};

class HAService {
public:
    static HAService& getInstance();
    
    // Initialize with base URL (e.g., http://raspberrypi:8123/) and Long-Lived Token
    bool begin(const String& baseUrl, const String& token);
    
    // Fetch shopping list from API
    bool fetchList();

    // Mark an item as complete or incomplete
    bool setComplete(const String& itemId, bool complete);
    
    // Getters for filtered lists
    const std::vector<HAShoppingItem>& getActiveItems() const { return _activeItems; }
    const std::vector<HAShoppingItem>& getCompletedItems() const { return _completedItems; }
    
    // For backward compatibility / all items if needed, but we focus on split views
    // const std::vector<HAShoppingItem>& getAllItems() const { return _items; } 

private:
    HAService() = default;
    
    String _baseUrl;
    String _token;
    bool _isInitialized = false;
    
    std::vector<HAShoppingItem> _activeItems;
    std::vector<HAShoppingItem> _completedItems;
};
