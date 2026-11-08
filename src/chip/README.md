Phase-by-phase; files land here as each capability is verified. The shared-I2C
and single-core helpers are hardware-verified. The SPI2 reset helper is compiled
against the pinned C5 core and comes from an accepted downstream integration;
the BSP integration still needs its own hardware re-acceptance. See
docs/HARDWARE-FINDINGS.md.
