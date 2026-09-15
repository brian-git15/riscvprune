#include "attendant.hpp"

#include "prizes.hpp"

namespace booth {

int CheerfulAttendant::tipOnWin(PrizeTier tier) const {
  return tierCost(tier) / 5;
}

const char *CheerfulAttendant::name() const { return "pixie"; }

int GrumpyAttendant::tipOnWin(PrizeTier tier) const {
  (void)tier;
  return 0;
}

const char *GrumpyAttendant::name() const { return "grump"; }

} // namespace booth
