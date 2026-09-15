#pragma once

#include "rng.hpp"
#include "types.hpp"

#include <stdexcept>

namespace booth {

struct JamError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Physical claw — can throw JamError on a rare jam.
class ClawMachine {
public:
  explicit ClawMachine(Rng &rng);

  ClawResult attemptGrab();
  int plays() const { return plays_; }

private:
  Rng &rng_;
  int plays_ = 0;
};

} // namespace booth
