#pragma once

#include <cstdint>

namespace booth {

// Deterministic LCG — leaf math, never throws.
class Rng {
public:
  explicit Rng(std::uint32_t seed = 0xC0FFEE);

  std::uint32_t next();
  int nextBounded(int exclusiveMax);

private:
  std::uint32_t state_;
};

} // namespace booth
