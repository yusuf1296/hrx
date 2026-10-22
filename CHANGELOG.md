# Changelog

Bu depoda yayımlanan sürümlerin geçmişi. Sürüm numaraları `osiris\version.txt` içinde belirlenir.

## 1.4.0 - 2026-10-22

İlk resmi yayın.

- **Player ESP** — Panorama tabanlı 2D düşman kutusu: bounding box (kamera sağ vektorüyle genişlik),
  can barı (HSB renkli dolgu) ve mesafe etiketi; ekranda olmayan/uzak hedefler otomatik gizlenir
- **LegitBunnyHop** — yere iniş algısı (z-ekseni konum farkı + zirve kontrolü) ile `user32!SendInput`
  üzerinden legit zıplama; basılı tuş hissi, inişte taze basım, havadayken sürdürme
- **Menü** — Combat > Movement (Bunny Hop) ve Visuals > Player ESP sekmeleri; Yes/No açılır-kapanır
  öğeleriyle tam yapılandırma entegrasyonu
- **Otomatik güncelleme** — injector sürüm kontrolü (GitHub), onaylı indirme, `.bak` yedekleme,
  `%APPDATA%` marker'ı ile menüde "Update available" uyarısı; ağ hatasında yerel sürümle devam
- **Altyapı** — CRT'siz karekök yaklaşımı (ILSqrt + Newton), NDC ham erişim (`getRawX/getRawY`),
  `WorldToClipSpaceConverter::rightVector()`, `Osiris.version` post-build kopyası, WinHTTP tabanlı
  injector updater'ı (`update.h`), tek tıkla yayın (`YAYINLA.bat`)
