# Osiris Updater — Teknik Referans

Otomatik güncelleme sisteminin tam spesifikasyonu. Akış özeti için [README](../README.md).

## Genel bakış

| Bileşen | Görev | Neden o? |
| --- | --- | --- |
| **OsirisInjector.exe** | Ağ erişimi, sürüm karşılaştırma, onay, DLL indirme ve değiştirme | Tam WinAPI erişimi var; başarısız olursa yerel DLL ile enjeksiyona devam eder |
| **Osiris.dll** | Sadece menüde "Update available" uyarısı gösterir | DLL CRT/heap/thread kullanmaz (manuel-map sözleşmesi); dosya ağı tamamen injector üstlenir |

Ara bağlantı: `%APPDATA%\OsirisCS2\update_available.txt` **marker** dosyası — injector yazar, DLL okur, güncelleme tamamlanınca injector siler.

## version.txt

Updater'ın tek girdisi. YAYINLA.ps1 tarafından üretilir (`osiris\version.txt` sürümünden).

| Satır | İçerik | Zorunlu |
| --- | --- | --- |
| 1 | Sürüm — `x.y.z` (rakamlar, isteğe bağlı `v` öneki) | evet |
| 2 | DLL adresi — mutlak URL veya göreli dosya adı | evet (yoksa indirme yapılmaz, sadece uyarı yazılır) |

- CRLF/LF farkı yoktur; satır sonları kırpılır.
- 2. satır göreli ise (`Osiris.dll`) 1. satırın URL'sinin bulunduğu dizine göre çözülür.
- Örnek (üretilen hâli):

  ```text
  1.4.0
  https://github.com/yusuf1296/osiris-updater/releases/download/v1.4.0/Osiris.dll
  ```

## Sürüm karşılaştırma

- Sayısal parçalar karşılaştırılır (`1.4.0` < `1.4.1` < `1.10.0`), `v` öneki yok sayılır.
- **Uzak = yerel** → `guncelleme yok`, eski marker temizlenir.
- **Uzak < yerel** (geliştirici yerelde eskide kaldıysa) → `yerel sürüm yayındakinden yeni`, indirme yapılmaz.
- Aynı sürüm tekrar yayınlanırsa **kimse indirmez** — sürüm her anlamlı değişiklikte artmalıdır.

## Injector bayrakları

```text
OsirisInjector.exe [dll-yolu] [--pid N | --process ad.exe] [--wait] [--keep-headers]
                   [--check-update] [--no-update] [--update-url url] [--help]
```

| Bayrak | Ne yapar |
| --- | --- |
| `--help`, `-h`, `/?` | Kullanım metnini basar |
| `--check-update` | Güncelleme akışını çalıştırır (gerekirse indirir), **enjeksiyon yapmadan çıkar** |
| `--no-update` | Güncellemeyi tamamen atlar (ini'ye rağmen) |
| `--update-url u` | ini'deki URL'yi geçersiz kılar (satır içi test için) |
| `--pid N` | Hedef PID |
| `--process ad.exe` | Hedef süreç adı (varsayılan `cs2.exe`) |
| `--wait` | Süreci 60 sn bekler |
| `--keep-headers` | Enjeksiyonda PE header'ları silmez (varsayılan: siler) |

`--check-update` hem kısayol (`--pid` vs. ile) hem de güncelleme kontrolü için aynı oturumda kullanılabilir; kontrol biter bitmez çıkış kodu ile döner.

## OsirisInjector.ini

Exe'nin yanına ilk çalıştırmada şablon otomatik yazılır.

```ini
; OsirisInjector otomatik guncelleme ayarlari
[update]
enabled=1
url=https://raw.githubusercontent.com/yusuf1296/osiris-updater/main/version.txt
```

| Anahtar | Varsayılan | Açıklama |
| --- | --- | --- |
| `enabled` | `1` | `0`/`false`/`no` → kontrol tamamen kapanır (`guncelleme kontrolu devre disi`) |
| `url` | (boş) | version.txt adresi; boşsa kontrol atlanır ve ipucu basılır |

Öncelik: **`--update-url` > `ini url` > (boş → atla)**. `--no-update` hepsinin üstünde.

## Güncelleme akışı

1. `enabled` ve `url` kontrol edilir; yan yana `Osiris.version` dosyası okunur (yoksa atla).
2. `version.txt` indirilir (en fazla birkaç KB); okunamazsa **yerel sürümle devam**.
3. Sürüm karşılaştırılır; uzak yeni değilse marker silinir ve biter.
4. Uzak yeniyse **marker yazılır** (DLL bundan sonra menüde uyarır) → `GUNCELLEME BULUNDU: 1.3.0 -> 1.4.0`.
5. Onay: `Indirilsin mi? [E/h]:` — **Enter/E = evet**, `h` = hayır (EOF da evet sayılır; pipe/CI dostu).
6. DLL indirilir → **PE x64** doğrulanır → `Osiris.dll.bak` yedeği alınır → değiştirilir → marker silinir.

Her adımda hata → indirme iptal edilir, **yerel DLL aynen kalır**, enjeksiyon devam eder.

## Marker dosyası

| | |
| --- | --- |
| Yol | `%APPDATA%\OsirisCS2\update_available.txt` |
| 1. satır | Uzak sürüm (`1.4.1`) |
| 2. satır | DLL adresi |
| Yazar | Injector (indirme reddedildiğinde / henüz inmemişken) |
| Okur | DLL — `PanoramaGUI::init()` → menüde turuncu **UpdateStatus** etiketi + tooltip |
| Siler | Injector (güncelleme başarılıysa ya da sürüm güncel bulunduysa) |

## Hata senaryoları (hepsi fail-safe)

| Durum | Davranış |
| --- | --- |
| İnternet yok / 404 / timeout | `guncelleme sunucusuna ulasilamadi, yerel surumle devam` |
| version.txt bozuk | `guncelleme dosyasi okunemedi, yerel surumle devam` |
| İndirilen dosya PE değil / x64 değil | `gecersiz PE (x64), yerel DLL ile devam` |
| 2. satır boş | İndirme yapılmaz, bilgi yazılır |
| `Osiris.version` yok | `Osiris.version bulunamadi (Osiris.dll yaninda), kontrol atlandi` |
| İndirme reddedildi (`h`) | Yerel DLL kalır, marker yazılı → menüde uyarı |

## Sorun giderme

| Belirti | Kontrol |
| --- | --- |
| Hiç güncelleme sormuyor | `OsirisInjector.ini` → `enabled=1` ve `url=` dolu mu? Sürüm dosyası `Osiris.version` DLL'in yanında mı? |
| `guncelleme URL'si ayarli degil` | ini'ye `url=` ekle (kurulumda otomatik olmalı) |
| 404 indirme | `version.txt` 2. satırı eski release'e mi bakıyor? YAYINLA.ps1 her yayınla yeniden üretir |
| Antivirüs siliyor | manual-map + indirilen DLL → AV allowlist'i gerekir; `--no-update` ile tamamen kapatılabilir |
| Uyarı menüde yok | Uyarı yalnızca **yeni sürüm varken** görünür; indirince injector marker'ı siler |

## CI

`.github/workflows/validate.yml` her push'ta şunları denetler:

1. `version.txt` — 1. satır `x.y.z`, 2. satır `https://…`
2. README'deki sürüm rozeti `version.txt` ile aynı sayıda
3. CHANGELOG'da sürüm bölümü varsa bilgi, yoksa uyarı (push'u engellemez)
