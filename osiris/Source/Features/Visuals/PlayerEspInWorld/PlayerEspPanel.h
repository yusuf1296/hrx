#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

#include <CS2/Classes/Color.h>
#include <CS2/Classes/Vector.h>
#include <CS2/Constants/ColorConstants.h>
#include <CS2/Panorama/CUILength.h>
#include <CS2/Panorama/CUIPanel.h>
#include <GameClient/Entities/TeamNumber.h>
#include <GameClient/Panorama/PanoramaLabel.h>
#include <GameClient/Panorama/PanoramaUiPanel.h>
#include <GameClient/WorldToScreen/WorldToClipSpaceConverter.h>
#include <Utils/ColorUtils.h>
#include <Utils/StringBuilder.h>

#include "PlayerEspInWorldConfigVariables.h"
#include "PlayerEspPanelParams.h"

template <typename HookContext>
class PlayerEspPanel {
public:
    PlayerEspPanel(HookContext& hookContext, cs2::CUIPanel* panelPointer) noexcept
        : hookContext{hookContext}
        , panelPointer{panelPointer}
    {
    }

    void update(auto&& playerPawn) const noexcept
    {
        if (!GET_CONFIG_VAR(player_esp_vars::Enabled)) {
            hide();
            return;
        }

        const auto absOrigin = playerPawn.absOrigin();
        if (!absOrigin.hasValue()) {
            hide();
            return;
        }

        const auto& origin = absOrigin.value();

        cs2::Vector head{origin.x, origin.y, origin.z + player_esp_panel_params::kPlayerHeightUnits};

        auto converter = hookContext.template make<WorldToClipSpaceConverter>();
        const auto feetClip = converter.toClipSpace(origin);
        const auto headClip = converter.toClipSpace(head);
        if (!feetClip.onScreen() || !headClip.onScreen()) {
            hide();
            return;
        }

        const auto feetNdc = feetClip.toNormalizedDeviceCoordinates();
        const auto headNdc = headClip.toNormalizedDeviceCoordinates();

        // Genislik: kamera sag vektoru kadar kaydirip tekrar projelestir.
        const auto rightVector = converter.rightVector();
        cs2::Vector side{origin.x + rightVector.x * player_esp_panel_params::kPlayerHalfWidthUnits,
            origin.y + rightVector.y * player_esp_panel_params::kPlayerHalfWidthUnits,
            origin.z + rightVector.z * player_esp_panel_params::kPlayerHalfWidthUnits};
        const auto sideClip = converter.toClipSpace(side);
        if (!sideClip.onScreen()) {
            hide();
            return;
        }
        const auto sideNdc = sideClip.toNormalizedDeviceCoordinates();

        const float halfWidthNdc = std::fabs(sideNdc.getRawX() - feetNdc.getRawX());
        const float boxWidthPercent = halfWidthNdc * 100.0f;
        const float centerXPercent = (feetNdc.getRawX() * 0.5f + 0.5f) * 100.0f;
        const float leftPercent = centerXPercent - boxWidthPercent * 0.5f;
        const float topPercent = (1.0f - (headNdc.getRawY() * 0.5f + 0.5f)) * 100.0f;
        const float heightPercent = std::fabs(headNdc.getRawY() - feetNdc.getRawY()) * 50.0f;

        if (boxWidthPercent < 0.05f || heightPercent < 0.05f) {
            hide();
            return;
        }

        // ekrann tamamen disinda kalan oyunculari gizle
        if (leftPercent > 100.0f || leftPercent + boxWidthPercent < 0.0f
            || topPercent > 100.0f || topPercent + heightPercent < 0.0f) {
            hide();
            return;
        }

        show();

        if (GET_CONFIG_VAR(player_esp_vars::BoundingBoxEnabled)) {
            updateBoundingBox(playerPawn, leftPercent, topPercent, boxWidthPercent, heightPercent);
        } else {
            boundingBoxPanel().setVisible(false);
        }

        if (GET_CONFIG_VAR(player_esp_vars::HealthBarEnabled)) {
            updateHealthBar(playerPawn, leftPercent, topPercent, heightPercent);
        } else {
            healthBarPanel().setVisible(false);
        }

        if (GET_CONFIG_VAR(player_esp_vars::DistanceEnabled)) {
            updateDistance(playerPawn, centerXPercent, topPercent, heightPercent);
        } else {
            distancePanel().setVisible(false);
        }
    }

private:
    void updateBoundingBox(auto&& playerPawn, float leftPercent, float topPercent, float widthPercent, float heightPercent) const noexcept
    {
        auto&& box = boundingBoxPanel();
        box.setVisible(true);
        box.setPosition(cs2::CUILength::percent(leftPercent), cs2::CUILength::percent(topPercent));
        box.setWidth(cs2::CUILength::percent(widthPercent));
        box.setHeight(cs2::CUILength::percent(heightPercent));
        box.setBorder(player_esp_panel_params::kBoxBorderWidth, teamColor(playerPawn));
    }

    void updateHealthBar(auto&& playerPawn, float leftPercent, float topPercent, float heightPercent) const noexcept
    {
        using namespace player_esp_panel_params;

        auto&& bar = healthBarPanel();
        bar.setVisible(true);
        bar.setPosition(cs2::CUILength::percent(leftPercent - kHealthBarGap.m_flValue - kHealthBarWidth.m_flValue),
            cs2::CUILength::percent(topPercent));
        bar.setWidth(kHealthBarWidth);
        bar.setHeight(cs2::CUILength::percent(heightPercent));

        const auto health = std::clamp(playerPawn.health().valueOr(0), 0, 100);
        auto&& fill = healthBarFillPanel();
        fill.setVisible(true);
        fill.setHeight(cs2::CUILength::percent(static_cast<float>(health)));
        fill.setPosition(cs2::CUILength::percent(0), cs2::CUILength::percent(100.0f - static_cast<float>(health)));
        fill.setBackgroundColor(healthColor(health));
    }

    void updateDistance(auto&& playerPawn, float centerXPercent, float topPercent, float heightPercent) const noexcept
    {
        using namespace player_esp_panel_params;

        auto&& label = distancePanel();
        label.setVisible(true);
        label.setPosition(cs2::CUILength::percent(centerXPercent - kDistanceWidth.m_flValue * 0.5f),
            cs2::CUILength::percent(topPercent + heightPercent + 0.2f));
        label.setWidth(kDistanceWidth);
        label.setHeight(kDistanceHeight);

        const auto meters = distanceInMeters(playerPawn);
        if (meters < 0) {
            label.setVisible(false);
            return;
        }

        StringBuilderStorage<10> storage;
        StringBuilder builder = storage.builder();
        builder.put(static_cast<int>(meters)).put('m');
        label.clientPanel().template as<PanoramaLabel>().setText(builder.cstring());
    }

    [[nodiscard]] float distanceInMeters(auto&& playerPawn) const noexcept
    {
        const auto playerOrigin = playerPawn.absOrigin();
        if (!playerOrigin.hasValue())
            return -1.0f;

        const auto localOrigin = hookContext.activeLocalPlayerPawn().absOrigin();
        if (!localOrigin.hasValue())
            return -1.0f;

        const auto delta = playerOrigin.value() - localOrigin.value();
        // world birimi = inc -> metre
        return sqrtApprox(delta.squareLength()) / 39.3701f;
    }

    // CRT bagimliligi olmasin diye kabaca karekok (Newton-Raphson).
    [[nodiscard]] static float sqrtApprox(float value) noexcept
    {
        if (value <= 0.0f)
            return 0.0f;

        std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
        bits = 0x5f3759dfu - (bits >> 1);
        float estimate = std::bit_cast<float>(bits);
        estimate = 0.5f * (estimate + value / estimate);
        estimate = 0.5f * (estimate + value / estimate);
        return estimate;
    }

    [[nodiscard]] static cs2::Color teamColor(auto&& playerPawn) noexcept
    {
        switch (playerPawn.teamNumber()) {
        using enum TeamNumber;
        case TT: return cs2::kColorTeamTT;
        case CT: return cs2::kColorTeamCT;
        default: return cs2::kColorWhite;
        }
    }

    [[nodiscard]] static cs2::Color healthColor(int health) noexcept
    {
        const auto fraction = std::clamp(health, 0, 100) / 100.0f;
        return color::HSBtoRGB(color::Hue{color::kRedHue + (color::kGreenHue - color::kRedHue) * fraction},
            color::Saturation{0.7f}, color::Brightness{1.0f});
    }

    void show() const noexcept
    {
        panel().setVisible(true);
    }

    void hide() const noexcept
    {
        panel().setVisible(false);
    }

    [[nodiscard]] decltype(auto) panel() const noexcept
    {
        return hookContext.template make<PanoramaUiPanel>(panelPointer);
    }

    // Cocuk sirasi: 0 = kutu, 1 = can barı, 2 = mesafe
    [[nodiscard]] decltype(auto) boundingBoxPanel() const noexcept
    {
        return panel().children()[0];
    }

    [[nodiscard]] decltype(auto) healthBarPanel() const noexcept
    {
        return panel().children()[1];
    }

    [[nodiscard]] decltype(auto) healthBarFillPanel() const noexcept
    {
        return healthBarPanel().children()[0];
    }

    [[nodiscard]] decltype(auto) distancePanel() const noexcept
    {
        return panel().children()[2];
    }

    HookContext& hookContext;
    cs2::CUIPanel* panelPointer;
};
