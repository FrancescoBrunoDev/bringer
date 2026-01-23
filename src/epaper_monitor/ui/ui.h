#pragma once

/*
 * ui.h
 *
 * Semplice menu UI che usa l'OLED SSD1306 per la navigazione:
 *
 * - Pulsante A (Clear)  : scorre tra le voci del menu
 * - Pulsante B (Toggle) : seleziona la voce corrente
 * - Pulsante B (long)   : torna indietro / esce dal menu
 *
 * Inizialmente viene mostrato il menu 'Settings' con due voci:
 *  - Partial update ON/OFF
 *  - Full cleaning (avvia una pulizia completa / recovery clear)
 *
 * L'implementazione registra le callback sui pulsanti (attraverso il modulo
 * `controls`) e aggiorna l'OLED tramite le funzioni in `epaper_monitor/oled`.
 */

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inizializza la UI: registra callback dei pulsanti e mostra il menu iniziale.
void ui_init(void);

// Poll della UI: chiamare frequentemente (es. dal loop) per aggiornare orologio e stato WiFi
void ui_poll(void);

// Scorri al prossimo elemento del menu (pulsante A breve)
void ui_next(void);

// Seleziona l'elemento attualmente evidenziato (pulsante B breve)
void ui_select(void);

// Torna indietro / esci dal menu (pulsante B long press)
void ui_back(void);

// Introspection helpers for the web UI
int ui_getState(void);
int ui_getIndex(void);

#ifdef __cplusplus
} // extern "C"
#endif
