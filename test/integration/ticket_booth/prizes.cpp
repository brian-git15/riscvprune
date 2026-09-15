#include "prizes.hpp"

namespace booth {

const char *tierName(PrizeTier tier) {
  switch (tier) {
  case PrizeTier::Sticker:
    return "neon-sticker";
  case PrizeTier::Plush:
    return "pixel-plush";
  case PrizeTier::Mystery:
    return "mystery-capsule";
  }
  return "unknown";
}

int tierCost(PrizeTier tier) {
  switch (tier) {
  case PrizeTier::Sticker:
    return 5;
  case PrizeTier::Plush:
    return 25;
  case PrizeTier::Mystery:
    return 50;
  }
  return 0;
}

PrizeTier pickTier(int roll0to99) {
  if (roll0to99 < 60)
    return PrizeTier::Sticker;
  if (roll0to99 < 90)
    return PrizeTier::Plush;
  return PrizeTier::Mystery;
}

} // namespace booth
