#pragma once

#include <CS2/Panorama/CUILength.h>
#include <GameClient/Panorama/PanelFontParams.h>

namespace player_esp_panel_params
{
    // Oyuncu model yuksekligi (world birimi = inc). CS2 oyuncusu 72 birim.
    static constexpr float kPlayerHeightUnits{72.0f};
    // Omuz genisliginin yarisi (kutu genisligi buradan hesaplanir).
    static constexpr float kPlayerHalfWidthUnits{16.0f};

    // Kutu cizgisi
    static constexpr auto kBoxBorderWidth = cs2::CUILength::pixels(1.0f);

    // Can barı (viewport yuzdesi)
    static constexpr auto kHealthBarWidth = cs2::CUILength::percent(0.3f);
    static constexpr auto kHealthBarGap = cs2::CUILength::percent(0.3f);

    // Mesafe yazisi
    static constexpr auto kDistanceWidth = cs2::CUILength::percent(6.0f);
    static constexpr auto kDistanceHeight = cs2::CUILength::percent(2.2f);
    static constexpr auto kDistanceFont = PanelFontParams{
        .fontFamily = "Stratum2 Bold TF, 'Arial Unicode MS'",
        .fontSize = 14};
}
