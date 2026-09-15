#pragma once

#include "attendant.hpp"
#include "ledger.hpp"
#include "machine.hpp"
#include "rng.hpp"
#include "types.hpp"

namespace booth {

struct SessionStats {
  int attempts = 0;
  int wins = 0;
  int jams = 0;
  int tips = 0;
};

// Orchestrator: catches JamError, pays attendant tips via virtual dispatch.
class TicketBooth {
public:
  TicketBooth(int startingTickets, std::uint32_t seed, Attendant &attendant);

  SessionStats run(int plays);
  int ticketsLeft() const { return ledger_.balance(); }

private:
  Rng rng_;
  Ledger ledger_;
  ClawMachine machine_;
  Attendant &attendant_;
};

} // namespace booth
