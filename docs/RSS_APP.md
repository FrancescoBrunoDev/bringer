# RSS Feed App (NY Times)

## Panoramica

L'app RSS permette di leggere feed RSS direttamente sul dispositivo Bringer. L'implementazione attuale è configurata per leggere le notizie dalla **Home Page del New York Times**.

## Struttura

### Servizio RSS (`src/app/rss/`)

Il servizio RSS gestisce il fetching e il parsing dei feed:

- **`rss.h`** - Definisce le strutture dati (`RSSItem`, `RSSFeed`) e l'interfaccia del servizio
- **`rss.cpp`** - Implementa il fetching HTTP e il parsing XML dei feed RSS

#### Funzionalità principali:

- `fetchNYT(feed, maxItems)` - Recupera le ultime notizie dal NYT
- `fetchFeed(url, feed, maxItems)` - Metodo generico per qualsiasi feed RSS

### App NY Times (`src/app/ui/apps/nytimes.cpp`)

L'interfaccia utente è ottimizzata per la lettura su e-paper:

- **OLED**: Mostra i titoli delle notizie navigabili con i pulsanti
- **E-Paper**: Visualizza l'articolo completo con titolo, autore, descrizione e data
- **Navigazione**:
    - **Su/Giù**: Scorrono il testo dell'articolo sull'e-paper (Wrapped a 18 caratteri per leggibilità)
    - **Next/Prev**: Scorrono tra le notizie (nella lista titoli su OLED)
    - **Select**: Apre/Aggiorna la notizia corrente sull'e-paper
    - **Back** (Long Press): Torna alla lista titoli

## Come funziona

1. **Primo accesso**: L'app scarica automaticamente le ultime notizie.
2. **Navigazione**: Usa i pulsanti per scorrere tra i titoli su OLED.
3. **Visualizzazione**: Premi select per caricare l'articolo sull'e-paper.
4. **Lettura**: Usa i tasti Su/Giù per leggere tutto il contenuto se è lungo.

## Aggiungere nuovi feed RSS

Per aggiungere un'altra app (es. BBC News):

1. Clona `src/app/ui/apps/nytimes.cpp` in `bbc.cpp`
2. Modifica la funzione `fetch_data` per chiamare `RSSService::getInstance().fetchFeed("URL_FEED", ...)`
3. Registra la nuova app in `apps.h` e `registry.cpp`

## Caratteristiche del parser e Layout

- ✅ **Single Column Layout**: Testo e metadati sono incolonnati per evitare sovrapposizioni su schermi stretti (128px).
- ✅ **Text Wrapping**: Tutto il testo è diviso in righe da 18 caratteri.
- ✅ **HTML Cleaning**: Rimuove tag HTML e decodifica entità comuni.
- ✅ **Filtri**: Rimuove descrizioni inutili (es. "Comments").
