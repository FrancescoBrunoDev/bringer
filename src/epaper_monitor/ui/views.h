// Views public declarations
#pragma once

#include <stdint.h>

struct View {
  void (*render)(void);
  void (*onNext)(void);
  void (*onSelect)(void);
  void (*onBack)(void);
  void (*poll)(void);
};

// Extern view instances
extern const View VIEW_HOME;
extern const View VIEW_PLACEHOLDER;
extern const View VIEW_SETTINGS_OVERVIEW;
extern const View VIEW_SETTINGS_MENU;
