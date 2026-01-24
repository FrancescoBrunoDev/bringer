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
// - prevPin: GPIO del pulsante 'Prev' (default 14)
// - nextPin: GPIO del pulsante 'Next' (default 16)
// - confirmPin: GPIO del pulsante 'Confirm' (default 9)
// - debounceMs: tempo di debounce in ms (default 50)
// Nota: la chiamata è compatibile con la vecchia firma a 2 argomenti; se non viene fornito
// il terzo parametro, `confirmPin` assume il valore di default.
void controls_init(uint8_t prevPin = 14, uint8_t nextPin = 16, uint8_t confirmPin = 9, unsigned long debounceMs = 50);

// Dev'essere chiamata frequentemente dal loop principale per gestire il polling dei pulsanti
void controls_poll(void);

// Imposta le callback chiamate su pressione breve dei pulsanti (Prev / Next / Confirm)
void controls_setPrevCallback(controls_button_cb_t cb);
void controls_setNextCallback(controls_button_cb_t cb);
void controls_setConfirmCallback(controls_button_cb_t cb);

// (Opzionale) Callback per pressioni lunghe (long-press) - fallback globale
// Se non sono impostate callback long-press per i singoli pulsanti, verrà usata questa
void controls_setLongPressCallback(controls_button_cb_t cb);

// (Opzionale) Imposta callback per pressioni lunghe dei singoli pulsanti
void controls_setPrevLongCallback(controls_button_cb_t cb);
void controls_setNextLongCallback(controls_button_cb_t cb);
void controls_setConfirmLongCallback(controls_button_cb_t cb);

// (Compat) Manteniamo i nomi storici per compatibilità; sono semplici wrapper
void controls_setClearCallback(controls_button_cb_t cb);
void controls_setToggleCallback(controls_button_cb_t cb);
void controls_setClearLongCallback(controls_button_cb_t cb);
void controls_setToggleLongCallback(controls_button_cb_t cb);

// (Opzionale) Imposta soglia long-press in ms (default 1000 ms)
void controls_setLongPressMs(unsigned long ms);

// Se true, il modulo registra comportamenti di default.
// Se false, solo le callback registrate verranno chiamate.
void controls_setUseDefaultActions(bool enable);

// Diagnostic helpers
// Return configured pins
uint8_t controls_getPrevPin(void);
uint8_t controls_getNextPin(void);
uint8_t controls_getConfirmPin(void);
// Return legacy names
uint8_t controls_getClearPin(void);
uint8_t controls_getTogglePin(void);
// Read raw digital state of a pin (convenience wrapper)
int controls_readPin(uint8_t pin);
