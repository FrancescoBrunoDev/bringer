#pragma once

#include <stdint.h>
#include <Arduino.h>

struct View {
  void (*render)(void);
  void (*onNext)(void);
  void (*onPrev)(void);
  void (*onSelect)(void);
  void (*onBack)(void);
  void (*poll)(void);
};

// Represents an App in the main menu carousel
struct App {
    // The view to render when this app is focused in the main menu
    // (Render the preview/widget)
    void (*renderPreview)(void);
    
    // Called when the user presses SELECT on this app in the main menu
    // Usually switches the current view to the app's internal view.
    void (*onSelect)(void);

    // Optional: called periodically (e.g. for background updates)
    void (*poll)(void);
};
