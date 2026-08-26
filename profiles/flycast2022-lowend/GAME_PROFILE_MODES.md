# Modalità dei profili Flycast per gioco

Opzione implementata:

```ini
retrorun_flycast_game_profile = disabled
```

Valori:

| Valore | Comportamento |
| --- | --- |
| `disabled` | Non applica il catalogo. Restano validi i parametri globali o scelti manualmente dall'utente. |
| `best_validated` | Usa la configurazione più veloce che ha superato i controlli visivi, audio e input. I profili catalogati che contengono soltanto il titolo ricevono il fallback conservativo; i giochi non catalogati restano invariati. |
| `best_performance` | Usa la configurazione più veloce conservata, anche se ha un difetto grafico documentato. Se non esiste una variante più aggressiva, usa `best_validated`. |

## Configurazioni attuali

La selezione deve usare il Product number Dreamcast, non il nome del file ROM.

| Gioco | Product number | `best_validated` | `best_performance` | Differenza nota |
| --- | --- | --- | --- | --- |
| Sonic Adventure 2 | `MK-51117`, `HDR-0165` | `fast_depth=menu_guarded_shadow_safe`, merge opachi disattivato | Merge opachi attivato, buffer 2048 e audio `lowend_stable_96` | Le varianti retail europea/nordamericana e giapponese condividono il profilo. Il confronto C1 da 300 frame ha conservato 299 frame presentati, aumentato il throughput da 33,47 a 36,29 FPS e ridotto gli underrun da 19 a 2. Ombre, menu, audio e gioco sono stati approvati manualmente. |
| Dead or Alive 2 | `RDC-0140`, `RDC-0149`, `T8116D 50`, `T3602M`, `T3601M`, `T3601N` | Profilo DOA2 corrente | Uguale a `best_validated` | Le varianti CDI osservate e le varianti retail regionali condividono il profilo. L'associazione di `RDC-0140` ha misurato circa 41,2 FPS contro 31,6 FPS senza profilo. Video, audio e comandi approvati. I tentativi audio/AICA più aggressivi non hanno dato un guadagno stabile. |
| Soul Calibur | `T1401D  50`, `T1401M`, `T1401N` | `retrorun-soul-T1401D50.cfg` | `experimental/retrorun-soul-performance-T1401D50.cfg` | Le edizioni retail europea, giapponese e nordamericana usano lo stesso profilo. Entrambe usano `framerate=normal` e disabilitano il pacing del frontend: su RG353M la configurazione ottimizzata con `fullspeed` era più lenta perfino della configurazione non ottimizzata con `normal`. La variante validata usa `per_triangle`; quella veloce usa `top_hud_last` e può mostrare il fondale sopra la barra. |
| Crazy Taxi | `MK-51035`, `HDR-0053` | B4: v9, audio `lowend_stable_96`, `per-triangle` e menu guard | Fallback a `best_validated` | Le edizioni retail europea/nordamericana e giapponese condividono il profilo B4 approvato manualmente. Sul cold boot RG351V ha misurato 41,31 core FPS e 39,04 frame unici/s contro 31,12 e 25,73 dello stack AmberELEC, con skip 5,5% contro 17,3% e un underrun contro 99. |
| Ikaruga | `T38706M` | D: v9, audio `lowend_stable_96`, core adaptive e `per-triangle` | Fallback a `best_validated` | La candidata D elimina i rettangoli attorno alle astronavi senza perdere prestazioni: 40,36 core FPS e 304 frame presentati contro 39,91 e 303 della candidata veloce originale. Video, audio e gameplay approvati manualmente su RG351V. |
| Jet Set Radio / Jet Grind Radio | `MK-51058` | `per-triangle`, merge traslucido off, fast depth e merge opaco, audio accurato con `lowend_stable_96` | Fallback a `best_validated` | Validato manualmente su Jet Grind Radio USA e RG351MP/dArkOS. Tre run da 600 frame sono passate da 25,99 a 27,86 frame presentati/s (+7,18%); p95 da 61,72 a 59,70 ms e zero underrun/overrun/drop audio. `per-strip`, merge traslucido e state elision sono risultati più lenti. |
| Cannon Spike | `T1215N` | `per-triangle`, merge traslucido off, fast depth disattivato, merge opaco attivo e audio accurato | Fallback a `best_validated` | Validato manualmente su RG351MP/dArkOS. Tre run finali da 600 frame hanno raggiunto 38,00 frame presentati/s contro 33,56 della baseline (+13,2%), senza frame persi o errori audio. Fast depth arrivava a circa 38,43 FPS da solo e 41,45 in combinazione, ma è stato respinto per artefatti grafici evidenti. |
| Daytona USA 2001 / Daytona USA | `MK-51037` | alpha `per-strip`, merge traslucido off, fast depth disattivato, merge opaco attivo e audio accurato | Fallback a `best_validated` | Validato manualmente durante una gara su RG351MP/dArkOS. Tre run finali da 600 frame hanno raggiunto 23,75 frame presentati/s contro 16,20 della baseline (+46,6%); p95 da 89,27 a 58,52 ms, tutti i frame presentati e nessun errore audio. La variante giapponese `HDR-0106` resta baseline finché non viene provata separatamente. |
| Street Fighter III: 3rd Strike | `T7013D50`, `T1213N`, `T1209M` | alpha `per-triangle`, merge traslucido e riuso texture off, fast depth `vertex_fast_log`, merge opaco attivo e audio accurato | Eredita il rispettivo `best_validated` | Il profilo USA è stato validato manualmente su RG351MP/dArkOS da un savestate di combattimento e poi associato alle tre varianti retail regionali con profili distinti ma identiche impostazioni, come per gli altri giochi multiregione. Tre run finali da 600 frame hanno raggiunto una mediana di 47,76 FPS contro 44,39 (+7,58%); active-frame p95 è sceso da 43,63 a 41,43 ms, senza underrun o code audio vuote. Alpha `per-strip` è stato respinto perché più lento e graficamente non sicuro. |

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
| Ikaruga | validata | off/off | `menu_guarded`, `scored`, alpha `per-triangle` | `vertex_fast_log` | `lowend_stable_96`, mixer `fast` | on | D24S0 |
| Jet Grind Radio | validata | on/on | merge off, alpha `per-triangle` | `enabled` | `lowend_stable_96`, mixer `accurate` | on | D24S0 |
| Cannon Spike | validata | on/on | merge off, alpha `per-triangle` | off | `lowend_stable_96`, mixer `accurate` | on | D24S0 |
| Daytona USA | validata | on/on | merge off, alpha `per-strip` | off | `lowend_stable_96`, mixer `accurate` | on | D24S0 |
| Shenmue II (Europe) | validata | on/on | merge off, alpha `per-strip` | `vertex_fast_log` | `lowend_heavy_100`, mixer `accurate` | off | D24S0 |
| Street Fighter III: 3rd Strike | validata | on/on | merge off, alpha `per-triangle` | `vertex_fast_log` | `lowend_stable_96`, mixer `accurate` | on | D24S8 |

## Regola di precedenza

1. Con `disabled`, un file per-gioco creato esplicitamente dall'utente resta
   completamente invariato.
2. Con una delle due modalità attive, RetroRun chiede al core Flycast il
   Product number prima di `retro_load_game()` e sovrascrive soltanto i
   parametri presenti nel profilo.
3. I profili con impostazioni esplicite conservano i default low-end usati
   durante la loro validazione; le voci baseline contenenti soltanto il titolo
   usano invece alpha `per-triangle`, merge traslucido/opaco, fast depth e
   texture-storage reuse disattivati. Le opzioni audio non vengono cambiate
   rispetto al precedente baseline low-end.
4. Un gioco sconosciuto o privo di Product number non riceve alcuna modifica:
   continuano a valere tutti i valori del `retrorun.cfg`.
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
