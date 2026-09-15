#include "booth.hpp"

#include "prizes.hpp"

namespace booth {

TicketBooth::TicketBooth(int startingTickets, std::uint32_t seed,
                         Attendant &attendant)
    : rng_(seed), ledger_(startingTickets), machine_(rng_),
      attendant_(attendant) {}

SessionStats TicketBooth::run(int plays) {
  SessionStats stats;

  for (int i = 0; i < plays; ++i) {
    const int cost = 10;
    if (!ledger_.spend(cost))
      break;

    ++stats.attempts;
    try {
      const ClawResult r = machine_.attemptGrab();
      if (r.grabbed) {
        ++stats.wins;
        const int tip = attendant_.tipOnWin(r.tier);
        stats.tips += tip;
        // Consolation: refund a slice of prize value as tickets.
        ledger_.earn(tierCost(r.tier) / 2);
      }
    } catch (const JamError &) {
      ++stats.jams;
      ledger_.earn(cost); // refund on jam
    }
  }

  return stats;
}

} // namespace booth
