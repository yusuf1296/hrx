# Katkıda Bulunma

Kısa ve net kurallar:

## Derleme

| Parça | Komut |
| --- | --- |
| DLL (Osiris) | `osiris\Osiris.sln` → **Release \| x64** (ya da MSBuild) |
| Injector | `osiris-injector\build.bat` (tek çeviri birimi, `winlearn`/`winhttp` pragma'lı) |

## Değişiklik gönderirken (PR / push)

1. **Sürümü unutma:** kullanıcıya giden her değişiklikte `osiris\version.txt` sayısını artır — sürüm aynıysa injector kimseye güncelleme sormaz.
2. **CHANGELOG:** yeni sürüm için `CHANGELOG.md` içine `## [x.y.z] - YYYY-MM-DD` bölümü aç.
3. **Temiz ağacın dışına çıkma:** `x64/`, `.vs/`, `*.obj`, `*.user` commit edilmez (`.gitignore` zaten koruyor).
4. Kök dosyaları (`README.md`, `version.txt`, `YAYINLA.ps1` …) elle değiştirdiysen `YAYINLA.bat` ile bir kez daha çalıştırıp rozetin senkron kaldığını doğrula.

## Kod stili

- Mevcut dosyanın stiline uy (Osiris upstream: `camelCase` yerel değişkenler, `PascalSize` metotlar).
- DLL tarafında **CRT/heap/thread yok** — `Osiris.dll` yalnızca Windows syscall + statik veri kullanır (upstream sözleşmesi).
- Yorumları kodun kendisi kadar kısa tut.

## Sorun bildirirken

`Hata bildir` şablonunu kullan; sürümü (`version.txt` / menü başlığı), Windows ve antivirüs bilgisini ekle.
