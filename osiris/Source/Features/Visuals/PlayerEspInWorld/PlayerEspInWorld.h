#pragma once

#include <Features/Common/InWorldPanels.h>
#include <GameClient/WorldToScreen/WorldToClipSpaceConverter.h>

#include "PlayerEspInWorldConfigVariables.h"

template <typename HookContext>
class PlayerEspInWorld {
public:
    PlayerEspInWorld(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void drawPlayerEsp(auto&& playerPawn) noexcept
    {
        if (!enabled() || !shouldDrawEspOnPawn(playerPawn))
            return;

        const auto absOrigin = playerPawn.absOrigin();
        if (!absOrigin.hasValue())
            return;

        const auto positionInClipSpace = hookContext.template make<WorldToClipSpaceConverter>().toClipSpace(absOrigin.value());
        if (!positionInClipSpace.onScreen())
            return;

        auto&& playerEspPanel = hookContext.template make<InWorldPanels>().getNextPlayerEspPanel();
        playerEspPanel.update(playerPawn);
    }

private:
    [[nodiscard]] bool enabled() const noexcept
    {
        return GET_CONFIG_VAR(player_esp_vars::Enabled);
    }

    [[nodiscard]] bool shouldDrawEspOnPawn(auto&& playerPawn) const noexcept
    {
        return playerPawn.isAlive().value_or(true)
            && playerPawn.health().greaterThan(0).valueOr(true)
            && !playerPawn.isControlledByLocalPlayer()
            && playerPawn.isTTorCT()
            && (!GET_CONFIG_VAR(player_esp_vars::OnlyEnemies) || playerPawn.isEnemy().value_or(true));
    }

    HookContext& hookContext;
};
