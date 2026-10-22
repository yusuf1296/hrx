# osiris-updater

Osiris otomatik guncelleme dosyalari.

- `version.txt` — 1. satir: surum (orn. `1.4.0`), 2. satir: DLL adi/URL'si (`Osiris.dll`)
- `Osiris.dll` — guncel surumun DLL dosyasi

`OsirisInjector.exe` bu depodaki `version.txt`'yi okur; yerel surumden yeni bir surum varsa
sorarak DLL'i indirir ve degistirir.

Yayin akisi: surum artirilir -> `version.txt` + `Osiris.dll` guncellenir -> commit + push.
