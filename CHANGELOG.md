# Changelog

Bu depoda yayımlanan sürümlerin geçmişi.
Biçim: [Keep a Changelog](https://keepachangelog.com/tr/1.1.0/) ·
Sürümlendirme: [Semantic Versioning](https://semver.org/lang/tr/).
Sürüm numaraları `osiris\version.txt` içinde belirlenir.

## [Unreleased]

## [1.5.0] - 2026-10-22

Depo yeniden adlandırıldı: **HRX** — telemetry ve canlı yönetim paneli eklendi.

### Added

- **Telemetry (DLL tarafı)** — DLL oyuna enjekte olduğu andan itibaren Supabase `checkins`
  tablosuna check-in gönderir: PC adı, Windows kullanıcı adı, Steam kullanıcı adı, sürüm,
  oyun-içi durum ve zaman damgası; ilk gönderim init anında, sonrasında **5 dakikada bir**
  asenkron WinHTTP ile (render karesini kilitlemez, worker thread tamamlar)
- **Telemetry kill switch** — `%APPDATA%\HRX\telemetry.off` dosyası varsa telemetry tamamen
  devre dışı; başarısız istekler üst üste 5'e ulaştığında otomatik durur (ağ yok → oyun etkilenmez)
- **panel.html (canlı panel)** — `panel.html`'i tarayıcıda aç: 5 dakikada aktif kullanıcılar,
  benzersiz PC sayısı, sürüm dağılımı, Steam adları ve release indirme sayıları; 15 sn
  otomatik yenileme (Supabase service anahtarı ilk açılışta sorulur, localStorage'da saklanır)
- **CI: telemetry sürüm senkronu** — `Telemetry.h` `kVersion` değeri `version.txt` ile
  eşleşmezse push CI'dan geçemez

### Changed

- **Depo adı** `yusuf1296/osiris-updater` → **`yusuf1296/hrx`** (eski URL'ler GitHub
  yönlendirmesiyle çalışmaya devam eder)
- **Injector çıktı adı** `OsirisInjector.exe` → **`HRX.exe`** (`build.bat`); `Osiris.dll`
  adı geri uyumluluk için **korundu** — mevcut kullanıcılar güncellemeyle telemetry'i otomatik alır
- Marka: README (TR/EN), banner, yayın konsolu başlığı ve release adları artık **HRX**
- WinAPI erişimi: forwarder-aware export resolver (kernel32 → kernelbase yönlendirmelerini
  takip eder), PEB üzerinden dinamik yükleme — import tablosuna yeni DLL bağımlılığı eklenmez

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

[Unreleased]: https://github.com/yusuf1296/hrx/compare/v1.5.0...HEAD
[1.5.0]: https://github.com/yusuf1296/hrx/releases/tag/v1.5.0
[1.4.0]: https://github.com/yusuf1296/hrx/releases/tag/v1.4.0
