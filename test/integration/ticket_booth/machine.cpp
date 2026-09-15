#include "machine.hpp"

#include "prizes.hpp"

namespace booth {

ClawMachine::ClawMachine(Rng &rng) : rng_(rng) {}

ClawResult ClawMachine::attemptGrab() {
  ++plays_;

  // 1-in-40 jam: keeps a throwing edge in the call graph.
  if (rng_.nextBounded(40) == 0)
    throw JamError("claw jammed on glitter pile");

  const int roll = rng_.nextBounded(100);
  const PrizeTier tier = pickTier(roll);
  const bool grabbed = rng_.nextBounded(3) != 0; // ~66% success

  ClawResult r;
  r.grabbed = grabbed;
  r.tier = tier;
  r.label = grabbed ? tierName(tier) : "empty-air";
  return r;
}

} // namespace booth
