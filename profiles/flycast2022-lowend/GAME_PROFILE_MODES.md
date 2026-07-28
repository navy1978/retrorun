# Modalità dei profili Flycast per gioco

Opzione implementata:

```ini
retrorun_flycast_game_profile = disabled
```

Valori:

| Valore | Comportamento |
| --- | --- |
| `disabled` | Non applica il catalogo. Restano validi i parametri globali o scelti manualmente dall'utente. |
| `best_validated` | Usa la configurazione più veloce che ha superato i controlli visivi, audio e input. Se il gioco non è catalogato, non modifica nulla. |
| `best_performance` | Usa la configurazione più veloce conservata, anche se ha un difetto grafico documentato. Se non esiste una variante più aggressiva, usa `best_validated`. |

## Configurazioni attuali

La selezione deve usare il Product number Dreamcast, non il nome del file ROM.

| Gioco | Product number | `best_validated` | `best_performance` | Differenza nota |
| --- | --- | --- | --- | --- |
| Sonic Adventure 2 | `MK-51117`, `HDR-0165` | `fast_depth=menu_guarded_shadow_safe`, merge opachi disattivato | Merge opachi attivato, buffer 2048 e audio `lowend_stable_96` | Le varianti retail europea/nordamericana e giapponese condividono il profilo. Il confronto C1 da 300 frame ha conservato 299 frame presentati, aumentato il throughput da 33,47 a 36,29 FPS e ridotto gli underrun da 19 a 2. Ombre, menu, audio e gioco sono stati approvati manualmente. |
| Dead or Alive 2 | `RDC-0140`, `RDC-0149`, `T8116D 50`, `T3602M`, `T3601M`, `T3601N` | Profilo DOA2 corrente | Uguale a `best_validated` | Le varianti CDI osservate e le varianti retail regionali condividono il profilo. L'associazione di `RDC-0140` ha misurato circa 41,2 FPS contro 31,6 FPS senza profilo. Video, audio e comandi approvati. I tentativi audio/AICA più aggressivi non hanno dato un guadagno stabile. |
| Soul Calibur | `T1401D  50`, `T1401M`, `T1401N` | `retrorun-soul-T1401D50.cfg` | `experimental/retrorun-soul-performance-T1401D50.cfg` | Le edizioni retail europea, giapponese e nordamericana usano lo stesso profilo. Entrambe usano `framerate=normal` e disabilitano il pacing del frontend: su RG353M la configurazione ottimizzata con `fullspeed` era più lenta perfino della configurazione non ottimizzata con `normal`. La variante validata usa `per_triangle`; quella veloce usa `top_hud_last` e può mostrare il fondale sopra la barra. |
| Crazy Taxi | `MK-51035`, `HDR-0053` | B4: v9, audio `lowend_stable_96`, `per-triangle` e menu guard | Fallback a `best_validated` | Le edizioni retail europea/nordamericana e giapponese condividono il profilo B4 approvato manualmente. Sul cold boot RG351V ha misurato 41,31 core FPS e 39,04 frame unici/s contro 31,12 e 25,73 dello stack AmberELEC, con skip 5,5% contro 17,3% e un underrun contro 99. |

## Parametri che differiscono davvero

I parametri non elencati restano quelli dei file `.cfg` presenti in questa
cartella.

| Gioco | Modalità | Mip/Fog | Traslucenze | Fast Depth | Audio | Opaque merge | EGL |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Sonic | validata | on/on | merge off | `menu_guarded_shadow_safe` | `lowend` | off | D24S8 |
| Sonic | prestazioni | on/on | merge off | `menu_guarded_shadow_safe` | `lowend` | on | D24S8 |
| DOA2 | entrambe | off/off | `menu_guarded`, `scored` | `vertex_fast_log` | `accurate` | on | D24S0 |
| Soul Calibur | validata | on/on | `menu_guarded`, `scored`, `per_triangle` | `vertex_fast_log` | `lowend` | off | predefinito |
| Soul Calibur | prestazioni | off/off | `menu_guarded`, `top_hud_last`, `standard` | `vertex_fast_log` | `lowend` | on | D24S0 |
| Crazy Taxi | validata | on/on | merge off, `top_hud_last`, `per-triangle` | `menu_guarded_shadow_safe` | `lowend_stable_96` | off | D24S8 |

## Regola di precedenza

1. Con `disabled`, un file per-gioco creato esplicitamente dall'utente resta
   completamente invariato.
2. Con una delle due modalità attive, RetroRun chiede al core Flycast il
   Product number prima di `retro_load_game()` e sovrascrive soltanto i
   parametri presenti nel profilo.
3. Le opzioni non specificate dal profilo continuano a provenire dalla
   configurazione normale.
4. Un gioco sconosciuto non riceve alcuna modifica automatica.
5. RetroRun usa il catalogo incorporato, oppure
   `flycast-game-catalog.ini` accanto all'eseguibile se ha un
   `catalog_version` strettamente maggiore ed è interamente valido.

Il catalogo esterno accetta soltanto una lista chiusa di impostazioni Flycast
e RetroRun. Non può contenere comandi o percorsi da eseguire.

Con `retrorun_flycast_catalog_update = auto` RetroRun controlla GitHub al
massimo una volta ogni 24 ore. Il controllo avviene in un processo separato e
non rallenta l'avvio del gioco. Un catalogo più nuovo viene validato, scritto
atomicamente come `flycast-game-catalog.cache.ini` accanto alla configurazione
attiva e usato dal lancio successivo. `disabled` disattiva il controllo remoto.

`best_performance` non significa “attiva tutte le opzioni veloci”: indica la
combinazione più rapida realmente conservata per quel gioco. In questo modo non
vengono riattivati esperimenti già risultati più lenti o instabili.
