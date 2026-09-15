#pragma once

#include "types.hpp"

namespace booth {

// Catalog lookups — no exceptions.
const char *tierName(PrizeTier tier);
int tierCost(PrizeTier tier);
PrizeTier pickTier(int roll0to99);

} // namespace booth
