# Future work (not the port)

Out of scope for this spec. Recorded so the port does not block it.

## FS9 support

Splits three ways, plus a flight model:

- **MDL8 model loader** (data). 38 of 40 MDLs in the FS9 corpus (`~/source/fs2004stuff/Aircraft`, 39 folders) are RIFF MDL8; 2 are FS98-style DLLs.
- **XML gauge interpreter** (data). 14 of the corpus panels are XML gauges in CABs.
- **FS9 C gauges** through the x86 guest, with a larger but bounded shim: statically linked MSVC CRT (~70 KERNEL32 functions), files, resources, DirectSound (8 call `DirectSoundCreate`), some `FindWindowA`/`LoadLibrary`/`GetProcAddress`. 27 in the corpus. Assumed to read sim state through the panel token API (`lookup_var`/`execute_calculator_code`), mapped to native values; verify against the FS9 SDK `gauges.h`.
- **Reimplemented FS9 stock gauges.** Most panels reference Microsoft stock gauges (Cessna, Bendix_King_Radio…) that ship with FS9, not the aircraft.
- **Optional FS9 flight model** for FS9 aircraft.

## Backlog

1. GPS output to the LAN for EFBs (ForeFlight, Garmin Pilot).
2. Airports generated from X-Plane free data (taxiways, signage).
3. Better terrain heights, batch-downloaded or streamed.
4. Satellite imagery, downloaded or streamed and converted.
5. Buildings from Microsoft building footprints.
6. Trees from coverage data.
7. Live ADS-B traffic, ~1 minute delayed for smooth interpolation.
8. Live shipping traffic (maybe).
9. Live METAR weather, with weather-model improvements if needed.
10. 3D blocky clouds visible at distance instead of 2D.
