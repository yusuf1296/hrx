# Changelog

Bu depoda yayımlanan sürümlerin geçmişi.
Biçim: [Keep a Changelog](https://keepachangelog.com/tr/1.1.0/) ·
Sürümlendirme: [Semantic Versioning](https://semver.org/lang/tr/).
Sürüm numaraları `osiris\version.txt` içinde belirlenir.

## [Unreleased]

## [1.4.0] - 2026-10-22

İlk resmi yayın.

### Added

- **Player ESP** — Panorama tabanlı 2D düşman kutusu: bounding box (kamera sağ vektorüyle genişlik),
  can barı (HSB renkli dolgu) ve mesafe etiketi; ekranda olmayan/uzak hedefler otomatik gizlenir
- **LegitBunnyHop** — yere iniş algısı (z-ekseni konum farkı + zirve kontrolü) ile `user32!SendInput`
  üzerinden legit zıplama; basılı tuş hissi, inişte taze basım, havadayken sürdürme
- **Menü** — Combat > Movement (Bunny Hop) ve Visuals > Player ESP sekmeleri; Yes/No açılır-kapanır
  öğeleriyle tam yapılandırma entegrasyonu
- **Otomatik güncelleme** — injector sürüm kontrolü (GitHub), onaylı indirme, `.bak` yedekleme,
  `%APPDATA%` marker'ı ile menüde "Update available" uyarısı; ağ hatasında yerel sürümle devam
- **Tek tıkla yayın** — `YAYINLA.bat`: sürüm dosyası + README rozeti + kaynak senkronu + Release
  açma/dosya yükleme + push tek adımda; push öncesi `validate` CI denetimi

### Infrastructure

- CRT'siz karekök yaklaşımı (ILSqrt + Newton), NDC ham erişim (`getRawX/getRawY`),
  `WorldToClipSpaceConverter::rightVector()`
- `Osiris.version` post-build kopyası; WinHTTP tabanlı injector updater'ı (`update.h`,
  AUTOMATIC_PROXY + redirect politikası, PE x64 doğrulama, göreli URL çözümleme)
- Docs: [docs/UPDATER.md](docs/UPDATER.md) teknik referans, issue/PR şablonları, CI

[Unreleased]: https://github.com/yusuf1296/osiris-updater/compare/v1.4.0...HEAD
[1.4.0]: https://github.com/yusuf1296/osiris-updater/releases/tag/v1.4.0
