#pragma once

/*
 * ui.h
 *
 * Semplice menu UI che usa l'OLED SSD1306 per la navigazione:
 *
 * - Pulsante Prev (short): scorre al precedente elemento del menu
 * - Pulsante Next (short): scorre al prossimo elemento del menu
 * - Pulsante Confirm: short press = seleziona/conferma, long press = annulla / torna indietro
 *
 * Inizialmente viene mostrato il menu 'Settings' con due voci:
 *  - Partial update ON/OFF
 *  - Full cleaning (avvia una pulizia completa / recovery clear)
 *
 * L'implementazione registra le callback sui pulsanti (attraverso il modulo
 * `controls`) e aggiorna l'OLED tramite le funzioni in `drivers/oled`.
 */

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inizializza la UI: registra callback dei pulsanti e mostra il menu iniziale.
void ui_init(void);

// Poll della UI: chiamare frequentemente (es. dal loop) per aggiornare orologio e stato WiFi
void ui_poll(void);

// Scorri al prossimo elemento del menu (pulsante Next breve)
void ui_next(void);

// Scorri al precedente elemento del menu (pulsante Prev breve)
void ui_prev(void);

// Seleziona l'elemento attualmente evidenziato / Conferma (pulsante Confirm breve)
void ui_select(void);

// Torna indietro / Annulla (pulsante Confirm long press)
void ui_back(void);

// Introspection helpers for the web UI
int ui_getState(void);
int ui_getIndex(void);
// Restituisce true se l'UI è attualmente nella schermata interna del Text App
bool ui_isInApp(void);

#ifdef __cplusplus
} // extern "C"
#endif
