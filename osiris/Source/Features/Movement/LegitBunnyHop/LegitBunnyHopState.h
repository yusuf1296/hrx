#pragma once

#include "LegitBunnyHopInput.h"

struct LegitBunnyHopState {
    LegitBunnyHopInput input{};
    bool jumpKeyHeldByUs{false};
    bool hasPreviousZ{false};
    float previousZ{0.0f};
    bool wasRising{false};
};
