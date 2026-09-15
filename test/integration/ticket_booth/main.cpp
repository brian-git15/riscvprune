// Multi-file encapsulating smoke for nounwind-lto.
// Theme: tiny arcade ticket booth with a jam-prone claw machine.
// Build IR + run pass via: test/integration/ticket_booth/run.sh

#include "attendant.hpp"
#include "booth.hpp"

#include <cstdio>

volatile int g_sink;

int main() {
  booth::CheerfulAttendant pixie;
  booth::TicketBooth arcade(/*tickets=*/200, /*seed=*/42, pixie);

  const booth::SessionStats s = arcade.run(/*plays=*/25);

  g_sink = s.attempts + s.wins + s.jams + s.tips + arcade.ticketsLeft();

  std::printf("attendant=%s attempts=%d wins=%d jams=%d tips=%d left=%d sink=%d\n",
              pixie.name(), s.attempts, s.wins, s.jams, s.tips,
              arcade.ticketsLeft(), g_sink);
  return 0;
}
