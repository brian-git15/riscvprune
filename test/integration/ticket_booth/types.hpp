#pragma once

#include <cstdint>

namespace booth {

enum class PrizeTier : std::uint8_t { Sticker, Plush, Mystery };

struct TicketBalance {
  int tickets = 0;
};

struct ClawResult {
  bool grabbed = false;
  PrizeTier tier = PrizeTier::Sticker;
  const char *label = "none";
};

} // namespace booth
