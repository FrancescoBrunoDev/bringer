#pragma once

/*
 * controls.h
 *
 * Gestione modulare dei pulsanti (Clear, Toggle Partial Update).
 *
 * - Debounce software
 * - Event callbacks (pressione breve)
 * - Comportamento di default: Clear -> epd_clear(), Toggle -> epd_setPartialEnabled(!...)
 *
 * Nota: i pulsanti sono gestiti come active-low (collegare l'altro piedino del pulsante a GND
 * e configurare il GPIO con INPUT_PULLUP).
 */

#include <Arduino.h>

using controls_button_cb_t = void (*)(void);

// Inizializza il gestore dei pulsanti.
// - clearPin: GPIO del pulsante 'Clear' (default 14)
// - togglePin: GPIO del pulsante 'Toggle Partial' (default 16)
// - debounceMs: tempo di debounce in ms (default 50)
void controls_init(uint8_t clearPin = 14, uint8_t togglePin = 16, unsigned long debounceMs = 50);

// Dev'essere chiamata frequentemente dal loop principale per gestire il polling dei pulsanti
void controls_poll(void);

// Imposta la callback chiamata su pressione breve del pulsante 'Clear'
void controls_setClearCallback(controls_button_cb_t cb);

// Imposta la callback chiamata su pressione breve del pulsante 'Toggle'
void controls_setToggleCallback(controls_button_cb_t cb);

// (Opzionale) Callback per pressioni lunghe (long-press) - fallback globale
// Questo mantiene la compatibilità con il comportamento precedente: se non sono
// impostate callback long-press per singolo pulsante, verrà usata questa.
void controls_setLongPressCallback(controls_button_cb_t cb);

// (Opzionale) Imposta callback per pressioni lunghe del pulsante 'Clear'
void controls_setClearLongCallback(controls_button_cb_t cb);

// (Opzionale) Imposta callback per pressioni lunghe del pulsante 'Toggle'
void controls_setToggleLongCallback(controls_button_cb_t cb);

// (Opzionale) Imposta soglia long-press in ms (default 1000 ms)
void controls_setLongPressMs(unsigned long ms);

// Se true, il modulo registra comportamenti di default (epd_clear / toggle partial).
// Se false, solo le callback registrate verranno chiamate.
void controls_setUseDefaultActions(bool enable);

// Diagnostic helpers
// Return configured pins
uint8_t controls_getClearPin(void);
uint8_t controls_getTogglePin(void);
// Read raw digital state of a pin (convenience wrapper)
int controls_readPin(uint8_t pin);
