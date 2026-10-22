#pragma once

#include <CS2/Constants/ColorConstants.h>
#include <CS2/Panorama/CUILength.h>
#include <CS2/Panorama/CUIPanel.h>
#include <CS2/Panorama/StyleEnums.h>
#include <GameClient/Panorama/PanelAlignmentParams.h>
#include <GameClient/Panorama/PanoramaUiPanel.h>
#include <Utils/Lvalue.h>

#include "PlayerEspPanelParams.h"

template <typename HookContext>
class PlayerEspPanelFactory {
public:
    explicit PlayerEspPanelFactory(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    // Butun ekrani kaplayan kap: cocuklar viewport yuzdesiyle konumlanir.
    [[nodiscard]] decltype(auto) createContainerPanel(auto&& parentPanel) const noexcept
    {
        auto&& uiPanel = hookContext.panelFactory().createPanel(parentPanel).uiPanel();
        uiPanel.setWidth(cs2::CUILength::percent(100));
        uiPanel.setHeight(cs2::CUILength::percent(100));
        uiPanel.setPosition(cs2::CUILength::percent(0), cs2::CUILength::percent(0));
        uiPanel.setFlowChildren(cs2::k_EFlowNone);
        return utils::lvalue<decltype(uiPanel)>(uiPanel);
    }

    // 2B kutu (border)
    void createBoundingBoxPanel(cs2::CUIPanel* parentPanel) const noexcept
    {
        auto&& uiPanel = hookContext.panelFactory().createPanel(parentPanel).uiPanel();
        uiPanel.setBorder(player_esp_panel_params::kBoxBorderWidth, cs2::kColorWhite);
    }

    // Can barı: arka plan + dolgu (dolgu bar'in cocugu)
    void createHealthBarPanel(cs2::CUIPanel* parentPanel) const noexcept
    {
        auto&& barPanel = hookContext.panelFactory().createPanel(parentPanel).uiPanel();
        barPanel.setBackgroundColor(cs2::kColorBlack);
        createHealthBarFillPanel(barPanel);
    }

    void createHealthBarFillPanel(cs2::CUIPanel* parentPanel) const noexcept
    {
        auto&& uiPanel = hookContext.panelFactory().createPanel(parentPanel).uiPanel();
        uiPanel.setBackgroundColor(cs2::kColorGreen);
    }

    // Mesafe yazisi
    void createDistancePanel(cs2::CUIPanel* parentPanel) const noexcept
    {
        using namespace player_esp_panel_params;

        auto&& label = hookContext.panelFactory().createLabelPanel(parentPanel).uiPanel();
        label.setFont(kDistanceFont);
        label.setColor(cs2::kColorWhite);
        label.setTextAlign(cs2::k_ETextAlignCenter);
        label.setAlign(PanelAlignmentParams{cs2::k_EHorizontalAlignmentCenter, cs2::k_EVerticalAlignmentCenter});
    }

private:
    HookContext& hookContext;
};
