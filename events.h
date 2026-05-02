#pragma once
// events.h - Special-event budget module
// Handles one-off budgets for things like trips, big shopping, etc.

#include <string>

namespace Events {
    void run(const std::string& userId);
}
