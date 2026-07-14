# Analisi delle prestazioni di RetroRun

Questo documento raccoglie i possibili colli di bottiglia individuati tramite
analisi statica del codice. Non rappresenta ancora un benchmark: prima di
applicare le modifiche più invasive sarà necessario misurare i tempi sui due
backend principali, SDL2 e GO2, e su core software e hardware differenti.

L'obiettivo non è rimuovere indiscriminatamente le attese per aumentare il
numero mostrato dal contatore FPS. RetroRun deve continuare a eseguire il core
alla frequenza dichiarata, mantenendo audio, video e input sincronizzati.
L'obiettivo è ridurre il tempo CPU/GPU impiegato per produrre ogni frame, il
jitter e la latenza audio.

## Priorità proposte

| Priorità | Area | Backend | Impatto atteso |
| --- | --- | --- | --- |
| 1 | Texture SDL persistente | SDL software | Molto alto |
| 2 | Ottimizzazione CRT/scanlines software | SDL e GO2 software | Molto alto con effetto attivo |
| 3 | Worker video persistente | SDL e GO2 | Alto se il multithread è attivo |
| 4 | Eliminazione del busy-wait audio | GO2 | Alto |
| 5 | Cache delle risorse e dello stato OpenGL | SDL OpenGL | Alto |
| 6 | Ring buffer e allocazioni audio | SDL | Medio/alto |
| 7 | Logging bufferizzato | SDL e GO2 | Alto con livello DEBUG |
| 8 | Calcolo FPS e pacing | SDL e GO2 | Basso/medio |

## 1. Texture SDL ricreata a ogni frame

Nel percorso software di `src/platform_sdl.cpp`, `render_surface()` esegue a
ogni frame alcune operazioni costose:

- crea un wrapper `SDL_Surface`;
- converte il formato RGB565 quando necessario;
- chiama `SDL_CreateTextureFromSurface()`;
- trasferisce l'intero framebuffer;
- distrugge subito la texture.

A 60 FPS questo produce almeno 60 creazioni e distruzioni di texture al
secondo, oltre alle conversioni e ai trasferimenti. È probabilmente il primo
collo di bottiglia da affrontare per SNES e per gli altri core software.

### Miglioramento proposto

- Conservare una texture `SDL_TEXTUREACCESS_STREAMING` persistente.
- Ricrearla solamente quando cambiano dimensioni o pixel format.
- Aggiornarla con `SDL_UpdateTexture()` oppure `SDL_LockTexture()`.
- Mantenere texture separate per menu e overlay.
- Aggiornare menu e overlay soltanto quando il loro contenuto cambia.

Rischio stimato: basso/medio. Impatto atteso: alto.

## 2. CRT e scanlines nel rendering software

In `src/video.cpp`, `drawNonOpenGL()` attraversa tutto il framebuffer a ogni
frame. Il ciclo interno usa operazioni floating point, divisioni, quadrati,
limiti e modulo per ogni pixel.

Un framebuffer 640x480 a 60 FPS richiede di elaborare oltre 18 milioni di
pixel al secondo, in aggiunta al lavoro del core. Il costo cresce rapidamente
con la risoluzione.

### Miglioramento proposto

- Precalcolare tabelle di intensità orizzontali e verticali quando cambia la
  risoluzione o l'effetto selezionato.
- Usare aritmetica intera fixed-point nel ciclo interno.
- Eliminare divisioni e modulo per pixel.
- Fondere copia, conversione del formato e applicazione dell'effetto in un
  unico passaggio.
- Valutare una versione GPU dell'effetto sui backend che la supportano.

Rischio stimato: medio, perché l'output grafico deve restare equivalente.
Impatto atteso: molto alto quando CRT o scanlines sono abilitati.

## 3. Creazione di un thread per ogni frame

Quando il rendering video multithread è abilitato, `src/video.cpp` crea un
nuovo `std::thread` per il singolo frame e usa `detach()`. Creare circa 60
thread al secondo può costare più del lavoro delegato.

Esiste inoltre un rischio di concorrenza: il buffer fornito dalla callback
libretro potrebbe non essere più valido dopo il ritorno della callback.

### Miglioramento proposto

- Usare un solo worker video persistente.
- Introdurre double o triple buffering.
- Usare una coda limitata a uno o due frame, evitando accumulo di latenza.
- Copiare esplicitamente il framebuffer quando la durata dei dati del core
  non è garantita.
- Lasciare disabilitato `retrorun_force_video_multithread` finché il percorso
  non viene convertito a un worker persistente.

Rischio stimato: medio/alto. Richiede test su SDL2 e su dispositivi GO2.

## 4. Busy-wait dell'audio GO2

In `src/go2/audio.cpp` OpenAL viene interrogato continuamente finché un buffer
non risulta processato. Questa attesa attiva può occupare completamente un
core della CPU.

Sui dispositivi portatili può causare maggiore consumo energetico, calore e
meno tempo CPU disponibile per l'emulazione.

### Miglioramento proposto

- Preferire un thread audio persistente e una condition variable.
- Gestire una coda di buffer OpenAL senza polling continuo.
- Come soluzione intermedia, introdurre `yield` o un breve backoff controllato.
- Misurare attentamente latenza, underrun e occupazione della coda sul device.

Rischio stimato: medio/alto, perché l'audio GO2 deve essere verificato su
hardware reale.

## 5. Risorse e stato OpenGL nel backend SDL

Il percorso OpenGL di `src/platform_sdl.cpp` crea texture e framebuffer
temporanei per alcuni overlay e successivamente li elimina. Durante la
presentazione vengono inoltre eseguite diverse `glGetIntegerv()` per salvare
lo stato OpenGL.

Le interrogazioni `glGet*` possono sincronizzare CPU e GPU. Le allocazioni di
texture e FBO diventano particolarmente costose quando menu oppure OSD sono
visibili.

### Miglioramento proposto

- Conservare texture, FBO, shader, VAO e VBO degli overlay.
- Aggiornare la texture dell'overlay soltanto quando è marcata `dirty`.
- Tenere traccia dello stato posseduto da RetroRun e ripristinare uno stato
  noto, riducendo le interrogazioni sincrone.
- Ricreare le risorse soltanto al cambio di contesto o dimensioni.

Rischio stimato: medio. Deve rispettare lo stato OpenGL lasciato e richiesto
dai diversi core hardware-rendered.

## 6. Coda e allocazioni audio SDL

In `src/platform_sdl.cpp`, l'invio audio interroga ripetutamente la dimensione
della coda e può effettuare più `SDL_Delay()`. Quando il volume è inferiore al
100%, viene anche costruito un nuovo `std::vector` per ogni blocco audio.

VSync, pacing del main loop e attese della coda audio possono rallentare
contemporaneamente lo stesso frame. Questa sovrapposizione può contribuire a
jitter, FPS inferiori al previsto oppure audio arretrato.

### Miglioramento proposto

- Riutilizzare un buffer persistente per la regolazione del volume.
- Introdurre un ring buffer audio.
- Valutare `SDL_AudioStream` o una callback SDL.
- Usare soglie minima e massima della coda invece di più attese consecutive.
- Stabilire un solo clock principale per il pacing; gli altri componenti
  devono limitarsi a prevenire overflow e underrun.

Rischio stimato: medio. Sono necessari test di latenza con VSync attivo e
disattivo.

## 7. Logging sincrono

`src/logger.cpp` usa più chiamate `printf()` e un `fflush(stdout)` per i
messaggi emessi. Con livello DEBUG e un core molto verboso, il terminale può
diventare un collo di bottiglia significativo.

La deduplicazione dei messaggi `Unhandled env` limita un caso specifico, ma
non elimina il costo degli altri messaggi prodotti frequentemente.

### Miglioramento proposto

- Comporre ogni messaggio ed emetterlo con una singola scrittura.
- Non eseguire `fflush()` per ogni messaggio DEBUG o INFO.
- Usare INFO o WARNING come livello predefinito nelle build release.
- Valutare un piccolo buffer o logger asincrono solo se le misurazioni ne
  dimostrano la necessità.
- Conservare ERROR e messaggi critici immediatamente visibili.

Rischio stimato: basso, purché i messaggi critici vengano scaricati subito e
il buffer sia svuotato correttamente all'uscita.

## 8. Copia audio personalizzata

`src/audio.cpp` contiene `newmemcpy()`, una copia manuale basata su blocchi da
64 bit. Le implementazioni `memcpy()` della libc sono normalmente ottimizzate
per la CPU in uso e possono selezionare automaticamente istruzioni più adatte.

### Miglioramento proposto

- Sostituire la funzione con `std::memcpy()` in un branch di prova.
- Confrontare entrambe le versioni con buffer delle dimensioni realmente
  prodotte dai core.
- Conservare la versione più veloce soltanto dopo una misura su ARM64 e x86-64.

Rischio stimato: basso. Impatto probabilmente piccolo o medio.

## 9. Calcolo degli FPS e pacing

Il main loop calcola statistiche FPS usando clock, divisioni e arrotondamenti
a ogni frame. Non è probabilmente un collo di bottiglia principale, ma non è
necessario aggiornare statistiche destinate alla visualizzazione 60 volte al
secondo.

Il percorso SDL usa deadline relativamente precise, mentre il percorso GO2
usa attese relative che possono accumulare deriva e jitter.

### Miglioramento proposto

- Aggiornare il contatore FPS ogni 250 o 1000 millisecondi.
- Usare `std::chrono::steady_clock` e deadline assolute anche su GO2.
- Recuperare il ritardo senza produrre raffiche incontrollate di frame.
- Evitare che VSync, audio e sleep del main loop diventino tre meccanismi di
  throttling indipendenti.

Questa modifica mira soprattutto alla regolarità dei frame, non ad aumentare
artificialmente gli FPS.

## 10. Altri punti da verificare

- `glReadPixels()` è sincrono e costoso: non deve essere usato nel normale
  percorso di gameplay. Per screenshot frequenti si possono riutilizzare i
  buffer o valutare PBO.
- Il wrapper della surface del contesto GO2 viene allocato e liberato in
  alcune operazioni di lock/unlock; può essere conservato nel contesto.
- Il polling input SDL non appare una priorità, ma le letture ripetute degli
  assi possono essere raccolte in un singolo snapshot per frame.
- Il marquee del menu usa strutture e calcoli non banali, ma è attivo soltanto
  durante la visualizzazione del menu e non è una priorità per il gameplay.
- L'attesa di mezzo secondo presente nel percorso di uscita non influenza il
  gameplay, ma rende l'arresto percepito come più lento e va riesaminata
  separatamente.

## Strumentazione proposta

Prima delle modifiche invasive conviene introdurre un profiler interno molto
leggero, disabilitato per default e attivabile da configurazione o build.
Dovrebbe accumulare i tempi senza produrre log per ogni frame e stampare un
riepilogo non più di una volta al secondo.

Tempi da misurare:

- `retro_run()`;
- callback e copia video;
- effetto CRT/scanlines;
- upload e present SDL;
- present OpenGL;
- invio audio e tempo di attesa della coda;
- polling input;
- sleep/VSync;
- tempo totale del frame;
- frame persi, audio underrun e profondità massima della coda.

Le statistiche dovrebbero includere almeno media, massimo e possibilmente il
95° percentile. La sola media può nascondere gli scatti occasionali.

## Matrice minima di test

Ogni ottimizzazione condivisa deve essere verificata almeno nei seguenti casi:

| Piattaforma | Tipo di core | Rendering | Effetti |
| --- | --- | --- | --- |
| macOS SDL2 | SNES o altro core software | Software/SDL | Off, scanlines, CRT |
| macOS SDL2 | Flycast | OpenGL | Off, scanlines, CRT |
| Linux ARM64 SDL2 | Core software | Software/SDL | Off e CRT |
| Linux ARM64 SDL2 | Core hardware | OpenGL ES | Off e CRT |
| Anbernic GO2 | Core software | DRM/RGA | Off e CRT |
| Anbernic GO2 | Core hardware compatibile | OpenGL ES/DRM | Off e CRT |

Per ogni prova annotare:

- frequenza dichiarata dal core e FPS effettivi;
- tempo medio e massimo del frame;
- utilizzo CPU per thread;
- latenza e stabilità audio;
- input latency percepita;
- presenza di tearing o stutter;
- temperatura e consumo, quando misurabili sul dispositivo.

## Ordine di implementazione consigliato

1. Aggiungere la strumentazione leggera e raccogliere una baseline.
2. Implementare la texture SDL streaming persistente.
3. Precalcolare le tabelle del filtro CRT/scanlines.
4. Eliminare la creazione di un thread per frame.
5. Correggere il busy-wait audio GO2.
6. Conservare le risorse OpenGL degli overlay.
7. Introdurre ring buffer e buffer volume persistente per SDL.
8. Ridurre il costo del logging nelle build release.
9. Applicare le ottimizzazioni minori soltanto se confermate dal profiler.

Ogni passaggio dovrebbe essere isolato in un commit e confrontato con la
baseline. In questo modo sarà possibile individuare facilmente eventuali
regressioni e preservare la compatibilità con i dispositivi Anbernic.
