#include "ledger.hpp"

namespace booth {

Ledger::Ledger(int startingTickets) { bal_.tickets = startingTickets; }

bool Ledger::canAfford(int cost) const { return bal_.tickets >= cost; }

bool Ledger::spend(int cost) {
  if (!canAfford(cost))
    return false;
  bal_.tickets -= cost;
  return true;
}

void Ledger::earn(int tickets) {
  if (tickets > 0)
    bal_.tickets += tickets;
}

} // namespace booth
