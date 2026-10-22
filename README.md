<div align="center">

# Osiris Updater

**Osiris CS2 için otomatik güncelleme merkezi** — sürüm dosyası, resmi indirmeler ve kaynak kod tek depoda.

![version](https://img.shields.io/badge/version-1.4.0-blue)
![license](https://img.shields.io/badge/license-MIT-green)
![downloads](https://img.shields.io/github/downloads/yusuf1296/osiris-updater/total-orange)
![last commit](https://img.shields.io/github/last-commit/yusuf1296/osiris-updater-purple)

[Releases](https://github.com/yusuf1296/osiris-updater/releases) · [Changelog](CHANGELOG.md) · [Kaynak Kod](./osiris)

</div>

---

## Nasıl çalışır?

Oyuncu hiçbir şeyle uğraşmaz; geliştirici tek dosyayı düzenler, tek bat'a çift tıklar.

```text
GELİŞTİRİCİ                         GİTHUB                        OYUNCU
-------------                       ------                        ------
osiris\version.txt -> 1.4.1
Release x64 derle
YAYINLA.bat
   │
   ├─ sürüm dosyasını üret ───────►  version.txt (1.4.1)
   ├─ kaynağı senkronla ──────────►  osiris/  injector/
   ├─ Release v1.4.1 aç ──────────►  Osiris.dll + OsirisInjector.exe
   └─ push ───────────────────────►
                                                       OsirisInjector.exe her açılışta
                                                       version.txt'yi okur
                                                                │
                                                        yeni sürüm varsa:
                                                        "1.4.0 -> 1.4.1
                                                         İndirilsin mi? [E/h]"
                                                                │
                                                        Evet → DLL indirilir,
                                                        .bak yedeği alınır,
                                                        yerine konur, iş biter.
                                                        Hayır → menüde turuncu
                                                        "Update available" uyarısı.
```

## Oyuncular için kurulum (ilk sefer)

1. [Releases](https://github.com/yusuf1296/osiris-updater/releases) sayfasından **OsirisInjector.exe** ve **Osiris.dll** dosyalarını aynı klasöre indir
2. `OsirisInjector.exe`'yi **Yönetici olarak çalıştır** (cs2 açıksa `--wait` ile bekleterek)
3. Bitti — bundan sonraki güncellemelerde injector kendisi sorar, "E" demen yeterli

> Sürüm kontrolü injector'ın ilk işidir; bağlantı yoksa yerel DLL ile devam eder, seni yarı yolda bırakmaz.

## Geliştirici için yayın akışı

1. `osiris\version.txt` içindeki sayıyı artır (`1.4.1`) — **her anlamlı değişiklikte şart**, aynı sürümde kimse indirmez
2. `osiris\Osiris.sln` → **Release | x64** derle
3. Kök klasördeki **`YAYINLA.bat`** dosyasına çift tıkla

Bat sırasıyla: README'deki sürüm rozetini günceller → kaynak kodu `osiris/` ve `injector/` altına senkronlar → GitHub Release'i (v1.4.1) oluşturup `Osiris.dll` + `OsirisInjector.exe` yükler → push eder.

## Depo yapısı

```text
osiris-updater/
├── version.txt          updater'ın okuduğu dosya: 1. satır sürüm, 2. satır DLL adresi
├── README.md            bu sayfa
├── CHANGELOG.md         sürüm geçmişi
├── LICENSE              MIT (upstream Osiris)
├── YAYINLA.ps1          yayın betiğini bat çağırır
├── osiris/              hile kaynağı — Osiris.sln + Source/ + Tests/
└── injector/            manuel-map injector kaynağı — build.bat ile derlenir
```

## SSS

**Güncelleme gelmiyor?**
- `OsirisInjector.ini` içinde `enabled=1` ve `url=` dolu olmalı (kurulumda otomatik yazılır)
- Injector'ı **yönetici** olarak çalıştır; internet kesikse sessizce yerel sürümle devam eder
- Sürüm dosyası: `version.txt` → 1. satır sürüm, 2. satır DLL adresi

**İndirilen DLL'i antivirüs siliyor?**
- Injector manual-map yapar, DLL oyun içine enjekte edilir → AV'ye allowlist/ekleme gerekir. İsteyen `--no-update` ile güncellemeyi tamamen kapatabilir.

**İndirme sorulmadan güncelsin istiyorum**
- Şu an tasarımda onay var ("İndirilsin mi? [E/h]"); Enter'a basmak da onaydır.

**Neden sürüm her seferinde artmalı?**
- Güncellemenin ölçütü sürümdür: dosya değişse de sürüm aynıysa injector indirme yapmaz.

## Uyarı

Bu depo bir eğitim/dağıtım altyapısıdır; Counter-Strike 2'nin kullanım koşulları hile kullanımını yasaklar ve hesap cezası doğurabilir. Riski bilerek kullan.

## Lisans

[MIT](LICENSE) — upstream [Osiris](https://github.com/danielkrupinski/Osiris) projesinin lisansı korunmuştur.
