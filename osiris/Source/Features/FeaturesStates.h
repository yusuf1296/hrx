#pragma once

#include "Combat/SniperRifles/NoScopeInaccuracyVis/NoScopeInaccuracyVisState.h"
#include "Hud/HudFeaturesStates.h"
#include "Movement/LegitBunnyHop/LegitBunnyHopState.h"
#include "Visuals/VisualFeaturesStates.h"

struct FeaturesStates {
    HudFeaturesStates hudFeaturesStates;
    VisualFeaturesStates visualFeaturesStates;
    NoScopeInaccuracyVisState noScopeInaccuracyVisState;
    LegitBunnyHopState legitBunnyHopState;
};
