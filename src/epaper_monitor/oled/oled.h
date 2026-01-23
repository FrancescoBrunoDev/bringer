#pragma once

/*
 * oled.h
 *
 * Helper per un piccolo display OLED SSD1306 (es. 128x64) con interfaccia I2C.
 *
 * - Inizializza il bus I2C e il display
 * - Fornisce API semplici per mostrare messaggi di stato (es. "Clearing", "Loading", "Done")
 * - Fornisce una funzione di fallback per verificare se il display è operativo
 *
 * Uso tipico:
 *   oled_init();                 // opzionale: passare SDA/SCL se non si usano i default
 *   oled_showStatus(\"Loading\");
 *   oled_clear();
 *
 * Nota: l'implementazione usa l'indirizzo I2C 0x3C di default (molti moduli SSD1306).
 */

#include <Arduino.h>
#include <stdint.h>



// Defaults (modificabili via parametri di oled_init)
static constexpr uint8_t OLED_DEFAULT_SDA = 15;       // suggerito: SDA -> GPIO15
static constexpr uint8_t OLED_DEFAULT_SCL = 22;       // suggerito: SCL -> GPIO22
static constexpr uint8_t OLED_DEFAULT_I2C_ADDR = 0x3C;
static constexpr uint8_t OLED_WIDTH = 128;
static constexpr uint8_t OLED_HEIGHT = 64;

/**
 * oled_init
 * Inizializza il bus I2C (Wire) e il display SSD1306.
 * - sda, scl: pin GPIO per SDA/SCL (Wire.begin(sda, scl))
 * - address: indirizzo I2C (tipicamente 0x3C)
 *
 * Dopo una inizializzazione fallita, oled_isAvailable() ritornerà false.
 */
void oled_init(uint8_t sda = OLED_DEFAULT_SDA,
               uint8_t scl = OLED_DEFAULT_SCL,
               uint8_t address = OLED_DEFAULT_I2C_ADDR);

/**
 * oled_isAvailable
 * Ritorna true se l'OLED è stato inizializzato correttamente e può essere usato.
 */
bool oled_isAvailable(void);

/**
 * oled_clear
 * Pulisce lo schermo (passata bianca).
 */
void oled_clear(void);

/**
 * oled_showStatus
 * Mostra un messaggio di stato centrato in carattere grande (es. \"Clearing\", \"Loading...\").
 * Viene eseguito immediatamente un display() per aggiornare lo schermo.
 */
void oled_showStatus(const char *msg);
inline void oled_showStatus(const String &msg) { oled_showStatus(msg.c_str()); }

/**
 * oled_showLines
 * Mostra due righe in carattere piccolo (utile per mostrare IP e un messaggio sotto).
 * Esegue il refresh immediato.
 */
void oled_showLines(const char *line1, const char *line2);
inline void oled_showLines(const String &l1, const String &l2) { oled_showLines(l1.c_str(), l2.c_str()); }

/**
 * oled_showProgress
 * Mostra un messaggio + progresso numerico (es. \"Clearing 1/4\").
 */
void oled_showProgress(const char *msg, int current, int total);
inline void oled_showProgress(const String &msg, int current, int total) { oled_showProgress(msg.c_str(), current, total); }

/**
 * oled_setMenuMode
 * Quando abilitato, l'OLED è riservato alla UI del menu e i messaggi di stato/progresso
 * provenienti da altri moduli (es. e-paper) verranno soppressi.
 */
void oled_setMenuMode(bool enable);

/**
 * oled_isMenuMode
 * Ritorna true se l'OLED è attualmente in modalità "menu".
 */
bool oled_isMenuMode(void);

/**
 * oled_showWifiIcon
 * Disegna una semplice icona WiFi (semicerchi + puntino) al centro del display.
 * - connected: se true, disegna i semicerchi normalmente; se false, disegna lo stesso
 *   ma l'icona può essere usata per indicare stato (caller decide il testo o serial fallback).
 */
void oled_showWifiIcon(bool connected);

/**
 * oled_showToast
 * Show a transient, non-persistent overlay message for `ms` milliseconds.
 * This does an immediate redraw of the OLED to display the toast. Call
 * `oled_poll()` frequently from the main loop (or ui_poll) to clear expired
 * toasts and allow the UI to redraw.
 */
void oled_showToast(const char *msg, uint32_t ms);

/**
 * oled_poll
 * Must be called frequently; returns true if a toast expired during this
 * call (so callers can trigger a UI redraw). If no toast is active it
 * returns false.
 */
bool oled_poll(void);
