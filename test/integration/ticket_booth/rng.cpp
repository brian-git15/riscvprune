#include "rng.hpp"

namespace booth {

Rng::Rng(std::uint32_t seed) : state_(seed ? seed : 1u) {}

std::uint32_t Rng::next() {
  // Numerical Recipes LCG constants.
  state_ = state_ * 1664525u + 1013904223u;
  return state_;
}

int Rng::nextBounded(int exclusiveMax) {
  if (exclusiveMax <= 0)
    return 0;
  return static_cast<int>(next() % static_cast<std::uint32_t>(exclusiveMax));
}

} // namespace booth
