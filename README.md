<div align="center">

<a href="https://github.com/yusuf1296/osiris-updater/releases">
  <img src="docs/images/banner.svg" alt="Osiris Updater" width="100%">
</a>

<br><br>

![version](https://img.shields.io/badge/version-1.4.0-blue)
![CI](https://github.com/yusuf1296/osiris-updater/actions/workflows/validate.yml/badge.svg)
![license](https://img.shields.io/badge/license-MIT-green)
![platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)
![downloads](https://img.shields.io/github/downloads/yusuf1296/osiris-updater/total-orange)
![last commit](https://img.shields.io/github/last-commit/yusuf1296/osiris-updater/purple)

<a href="https://github.com/yusuf1296/osiris-updater/releases/latest/download/OsirisInjector.exe"><img alt="OsirisInjector.exe indir" src="https://img.shields.io/badge/download-OsirisInjector.exe-2ea44f?style=for-the-badge"></a>
<a href="https://github.com/yusuf1296/osiris-updater/releases/latest/download/Osiris.dll"><img alt="Osiris.dll indir" src="https://img.shields.io/badge/download-Osiris.dll-2ea44f?style=for-the-badge"></a>

**English** · [Türkçe](README.md)

[Releases](https://github.com/yusuf1296/osiris-updater/releases) ·
[Changelog](CHANGELOG.md) ·
[Teknik referans](docs/UPDATER.md) ·
[Katkı](CONTRIBUTING.md) ·
[Kaynak Kod](./osiris)

</div>

---

## İçindekiler

- [Öne çıkanlar](#öne-çıkanlar)
- [Nasıl çalışır?](#nasıl-çalışır)
- [Güncelleme nasıl görünür?](#güncelleme-nasıl-görünür)
- [Mimari](#mimari)
- [Kurulum (oyuncu)](#kurulum-oyuncu)
- [Geliştirici: yayın akışı](#geliştirici-yayın-akışı)
- [Depo yapısı](#depo-yapısı)
- [Teknik özet](#teknik-özet)
- [SSS](#sss)
- [Uyarı](#uyarı)
- [Lisans](#lisans)

## Öne çıkanlar

| | |
| --- | --- |
| **Sürüm tabanlı otomatik güncelleme** | Injector her açılışta `version.txt` okur; yeni sürüm varsa sorar, indirir, `.bak` yedeği alır |
| **Onaylı indirme** | `İndirilsin mi? [E/h]` — evet dersen indirir, hayır dersen menüde turuncu uyarı kalır |
| **Fail-safe** | Ağ yok / dosya bozuk / PE değil → indirme iptal, **yerel DLL aynen kalır**, oyun yine açılır |
| **Tek tıkla yayın** | `YAYINLA.bat` → sürüm dosyası + rozet + kaynak senkronu + Release + push, tek adımda |
| **Denetimli push** | Her push `validate` CI'ından geçer: sürüm formatı, rozet senkronu, yasaklı dosya yokluğu |

## Nasıl çalışır?

Geliştirici tek dosyayı düzenler, tek bat'a çift tıklar; oyuncu yalnızca "E" der.

```mermaid
flowchart TD
  subgraph DEV["Geliştirici"]
    A["osiris/version.txt<br/>sürümü artır"] --> B["Release x64 derle"]
    B --> C["YAYINLA.bat"]
  end
  subgraph GH["GitHub"]
    D["version.txt<br/>1. satır sürüm · 2. satır DLL adresi"]
    E["osiris/ + injector/<br/>kaynak kod"]
    F["Release v1.x<br/>Osiris.dll + OsirisInjector.exe"]
  end
  subgraph PLY["Oyuncu"]
    G["OsirisInjector.exe açılır"] --> H{"uzak sürüm<br/>yerelden yeni mi?"}
    H -->|"evet"| I["İndirilsin mi? [E/h]<br/>E → .bak yedeği → değiştir"]
    H -->|"hayır"| J["menüde turuncu<br/>Update available"]
  end
  C --> D
  C --> E
  C --> F
  D --> G
  F --> I
```

- **Sürüm ölçütü dosyanın kendisidir:** dosya değişse de sürüm aynıysa injector indirme yapmaz.
- **Bağlantı yoksa kırılmaz:** ulaşılamazsa yerel DLL ile enjeksiyona devam eder.
- Ayrıntı: [docs/UPDATER.md](docs/UPDATER.md).

## Güncelleme nasıl görünür?

![Injector konsol çıktısı](docs/images/update-prompt.svg)

İndirme tamamlanınca eski DLL `Osiris.dll.bak` olarak kalır, `%APPDATA%` marker'ı silinir ve injector enjeksiyona geçer. `h` dersen indirme yapılmaz — oyuncu menüdeki turuncu **Update available** uyarısını görür, bir sonraki açılışta tekrar sorulur.

## Mimari

![Güncelleme mimarisi](docs/images/architecture.svg)

Ağ ve dosya işinin tamamı injector'da; DLL yalnızca marker'ı okuyup menüde uyarı gösterir (CRT/heap/thread kullanmadığı için). Hata senaryolarının tamamı [docs/UPDATER.md](docs/UPDATER.md#hata-senaryoları-hepsi-fail-safe) içinde.

## Kurulum (oyuncu)

1. Yukarıdaki **Download** düğmelerinden `OsirisInjector.exe` ve `Osiris.dll`'i **aynı klasöre** indir
2. `OsirisInjector.exe`'yi **Yönetici olarak çalıştır** (cs2 açıksa `--wait` bekleterek)
3. Bitti — sonraki güncellemelerde injector kendisi sorar, **Enter/E** demen yeterli

## Geliştirici: yayın akışı

1. `osiris\version.txt` içindeki sayıyı artır (`1.4.1`) — **her anlamlı değişiklikte şart**
2. `osiris\Osiris.sln` → **Release | x64** derle (injector değiştiyse `osiris-injector\build.bat` de)
3. Kökteki **`YAYINLA.bat`** dosyasına çift tıkla

```text
================================================================
  OSIRIS YAYIN  -  surum 1.4.0
  https://github.com/yusuf1296/osiris-updater
================================================================

  [1/7] surum dogrulandi: 1.4.0                                 OK
  [2/7] derleme bulundu: Osiris.dll (Release x64)               OK
  [3/7] version.txt + README rozetleri guncellendi              OK
  [4/7] kaynak senkron: osiris/ + injector/                     OK
  [5/7] commit: publish 1.4.0                                   OK
  [6/7] release v1.4.0: dll + exe hazir                         OK
  [7/7] push origin main                                        OK

----------------------------------------------------------------
  YAYIN TAMAM - surum 1.4.0
----------------------------------------------------------------
```

Yayın betiği her şeyi sırayla yapar: `version.txt` üretir (2. satır release adresi) → `README.md` **ve** `README.en.md` rozetlerini eşler → kaynağı derleme artıkları hariç senkronlar → `CHANGELOG.md`'den release notlarını çıkarıp **Release vX.Y.Z** açar → `Osiris.dll` + `OsirisInjector.exe` yükler → **en son** push eder.

## Depo yapısı

```text
osiris-updater/
├── README.md / README.en.md   bu sayfa (TR/EN)
├── version.txt                updater'ın okuduğu dosya: 1. satır sürüm, 2. satır DLL adresi
├── CHANGELOG.md               sürüm geçmişi (Keep a Changelog)
├── CONTRIBUTING.md            katkı ve PR kuralları
├── LICENSE                    MIT (upstream Osiris)
├── YAYINLA.ps1                yayın betiği (YAYINLA.bat çağırır)
├── .github/
│   ├── workflows/validate.yml     push başı format + senkron denetimi
│   └── ISSUE_TEMPLATE/            hazır hata bildirim formu
├── docs/
│   ├── UPDATER.md             updater teknik referansı (bayraklar, ini, marker)
│   └── images/                banner, mimari diyagram, konsol görüntüsü
├── osiris/                    hile kaynağı — Osiris.sln + Source/ + Tests/
└── injector/                  manuel-map injector kaynağı — build.bat ile derlenir
```

## Teknik özet

| Konu | Özet |
| --- | --- |
| `version.txt` | 1. satır `x.y.z`, 2. satır DLL adresi (mutlak veya göreli) |
| Onay | `İndirilsin mi? [E/h]` — Enter da onaydır; `h` → menüde uyarı kalır |
| Konfigürasyon | `OsirisInjector.ini` → `[update] enabled / url` (ilk açılışta otomatik oluşur) |
| Marker | `%APPDATA%\OsirisCS2\update_available.txt` — injector yazar, DLL okur |
| CLI | `--check-update`, `--no-update`, `--update-url`, `--pid`, `--process`, `--wait`, `--keep-headers` |
| Güvenlik ağı | Ağ/PE hatası → yerel DLL; başarılı güncelleme → `Osiris.dll.bak` |

Tam spesifikasyon: **[docs/UPDATER.md](docs/UPDATER.md)**.

## SSS

**Güncelleme hiç gelmiyor?**
`OsirisInjector.ini` içinde `enabled=1` ve `url=` dolu olmalı; injector'ı yönetici çalıştır. Ayrıntı: [sorun giderme](docs/UPDATER.md#sorun-giderme).

**İndirilen DLL'i antivirüs siliyor?**
Injector manual-map yapar → AV'ye allowlist/ekleme gerekir. `--no-update` ile güncellemeyi tamamen kapatabilirsin.

**Sorulmadan güncelsin istiyorum?**
Tasarımda onay var; hızlı yol: Enter'a basmak da onaydır.

**Neden sürüm her seferinde artmalı?**
Güncellemenin ölçütü sürümdür — dosya değişse de sürüm aynıysa injector indirme yapmaz.

## Uyarı

Bu depo bir eğitim/distribüsyon altyapısıdır; Counter-Strike 2'nin kullanım koşulları hile kullanımını yasaklar ve hesap cezası doğurabilir. Riski bilerek kullan.

## Lisans

[MIT](LICENSE) — upstream [Osiris](https://github.com/danielkrupinski/Osiris) projesinin lisansı korunmuştur.
