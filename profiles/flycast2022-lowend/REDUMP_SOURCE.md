# Dreamcast variant source

The retail release inventory used while maintaining
`dreamcast-product-variants.tsv` comes from:

```text
https://raw.githubusercontent.com/libretro/libretro-database/refs/heads/master/metadat/redump/Sega%20-%20Dreamcast.dat
```

The source version inspected for catalog `20260826` is `2026.08.01`.
`libretro-database` is distributed under CC BY-SA 4.0. This repository keeps a
small attributed cross-reference for the games that have RetroRun performance
profiles; it does not embed the complete upstream DAT.

The upstream `serial` field cannot be copied mechanically into the runtime
catalog. Some discs have more than one Redump serial, while Flycast selects a
profile using the fixed-width Product number stored in Dreamcast IP.BIN.
`dreamcast-product-variants.tsv` therefore records both values explicitly.

Maintenance rule:

1. Find the game's retail entries in the current upstream DAT.
2. Exclude demos, trials, betas and prototypes unless they were tested
   independently.
3. Record every retail region/revision in
   `dreamcast-product-variants.tsv`.
4. Add each corresponding normalized IP.BIN Product number to
   `flycast-game-catalog.ini`.
5. Increment `catalog_version` and run `make test`.

The catalog test checks that every Product number in the variant table exists
in the runtime catalog and that the editable catalog exactly matches the
built-in catalog.

The `2026.08.01` audit covered all 32 catalog game families. After adding the
four missing retail Product numbers for Power Stone, all retail entries were
accounted for by 94 runtime Product numbers. Nineteen matched demo, beta or
prototype entries were deliberately excluded under the maintenance rule
above. Alternate Redump serials with identical disc data remain mapped to one
IP.BIN Product number instead of creating duplicate runtime profiles.
