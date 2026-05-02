#pragma once
// split.h - Group expense splitting module
//
// Each group has plain-text member names (no need for them to be
// BudgetBuddy users). For every shared expense we record:
//   - who paid
//   - who was involved (only the involved members owe a share)
//   - the total amount
//   - the reason / description
//
// Settlement is computed on demand: for every member, work out the
// net balance, then greedily match creditors with debtors so we get
// the minimum number of payments to settle.

#include "models.h"
#include <string>
#include <vector>

namespace Split {
    // Compute the optimal repayment plan for a group.
    std::vector<Settlement> calcSettlements(const Group& g);

    // Enter the splitting submenu (main entry).
    void run(const std::string& userId);
}
