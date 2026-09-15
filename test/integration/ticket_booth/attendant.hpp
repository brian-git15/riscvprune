#pragma once

#include "types.hpp"

namespace booth {

// Virtual attendant roles — indirect calls stay conservative unless
// later LTO/devirt resolves them.
class Attendant {
public:
  virtual ~Attendant() = default;
  virtual int tipOnWin(PrizeTier tier) const = 0;
  virtual const char *name() const = 0;
};

class CheerfulAttendant : public Attendant {
public:
  int tipOnWin(PrizeTier tier) const override;
  const char *name() const override;
};

class GrumpyAttendant : public Attendant {
public:
  int tipOnWin(PrizeTier tier) const override;
  const char *name() const override;
};

} // namespace booth
