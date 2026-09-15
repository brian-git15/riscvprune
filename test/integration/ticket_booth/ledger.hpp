#pragma once

#include "types.hpp"

namespace booth {

// Ticket wallet — pure arithmetic, never throws.
class Ledger {
public:
  explicit Ledger(int startingTickets);

  int balance() const { return bal_.tickets; }
  bool canAfford(int cost) const;
  bool spend(int cost);
  void earn(int tickets);

private:
  TicketBalance bal_;
};

} // namespace booth
