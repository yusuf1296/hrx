#pragma once

#include <Config/ConfigVariable.h>

namespace player_esp_vars
{

CONFIG_VARIABLE(Enabled, bool, false);
CONFIG_VARIABLE(OnlyEnemies, bool, true);
CONFIG_VARIABLE(BoundingBoxEnabled, bool, true);
CONFIG_VARIABLE(HealthBarEnabled, bool, true);
CONFIG_VARIABLE(DistanceEnabled, bool, true);

}
