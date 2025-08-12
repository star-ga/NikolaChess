
#pragma once
#include <cstdint>

// Returns true on success (Linux), no-op elsewhere.
bool nikola_pin_thread_to_core(int coreIndex);
