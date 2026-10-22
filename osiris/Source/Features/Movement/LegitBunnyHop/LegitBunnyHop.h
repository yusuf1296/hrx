#pragma once

#include "LegitBunnyHopConfigVariables.h"
#include "LegitBunnyHopState.h"

template <typename HookContext>
class LegitBunnyHop {
public:
    LegitBunnyHop(HookContext& hookContext) noexcept
        : hookContext{hookContext}
    {
    }

    void run() const noexcept
    {
        if (!shouldPressJumpKey()) {
            resetJumpKey();
            return;
        }

        auto&& localPlayerPawn = hookContext.activeLocalPlayerPawn();
        if (!localPlayerPawn || !localPlayerPawn.isAlive().value_or(false)) {
            resetJumpKey();
            return;
        }

        const auto absOrigin = localPlayerPawn.absOrigin();
        if (!absOrigin.hasValue()) {
            resetJumpKey();
            return;
        }

        if (detectLanding(absOrigin.value().z))
            pressJumpKey();
    }

    void onUnload() const noexcept
    {
        resetJumpKey();
    }

private:
    [[nodiscard]] bool shouldPressJumpKey() const noexcept
    {
        return GET_CONFIG_VAR(legit_bunny_hop_vars::Enabled) && state().input.isJumpKeyHeld();
    }

    // Zemine degdigimizde yeni bir basinim olusturur, havadayken basili tutar.
    void pressJumpKey() const noexcept
    {
        state().input.repressJumpKey();
        state().jumpKeyHeldByUs = true;
    }

    void resetJumpKey() const noexcept
    {
        if (state().jumpKeyHeldByUs)
            state().input.releaseJumpKey();
        state().jumpKeyHeldByUs = false;
        state().hasPreviousZ = false;
        state().wasRising = false;
    }

    // Yer tespiti: dikey konum degisimi (hiz ≈ 0) ve onceki karede yukselmiyor olma.
    [[nodiscard]] bool detectLanding(float z) const noexcept
    {
        auto& currentState = state();

        bool onGround = false;
        if (currentState.hasPreviousZ) {
            const float deltaZ = z - currentState.previousZ;
            constexpr float kVerticalMovementThreshold{0.05f};
            const bool rising = deltaZ > kVerticalMovementThreshold;
            const bool falling = deltaZ < -kVerticalMovementThreshold;
            // zirvede hiz sifira yakin oldugundan "onceki karede yukseliyordu" kontrolu gerekir
            onGround = !rising && !falling && !currentState.wasRising;
            currentState.wasRising = rising || (currentState.wasRising && !falling);
        }

        currentState.previousZ = z;
        currentState.hasPreviousZ = true;
        return onGround;
    }

    [[nodiscard]] auto& state() const noexcept
    {
        return hookContext.featuresStates().legitBunnyHopState;
    }

    HookContext& hookContext;
};
