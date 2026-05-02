#pragma once
// storage.h - File I/O module
// All data is persisted to ./data/ as JSON, with legacy .txt fallback on load.

#include "models.h"
#include <string>
#include <vector>

namespace Storage {

    void init(); // Creates data/ directory (call once at startup)

    // --- Users ------------------------------------------
    std::vector<User>   loadUsers();
    void                saveUsers(const std::vector<User>& users);

    // --- Transactions -----------------------------------
    std::vector<Transaction> loadTransactions(const std::string& userId);
    void saveTransactions(const std::string& userId,
                          const std::vector<Transaction>& txs);

    // --- Categories -------------------------------------
    std::vector<Category> loadCategories(const std::string& userId);
    void saveCategories(const std::string& userId,
                        const std::vector<Category>& cats);

    // --- Budget Settings (one record per user-month) ----
    // Returns the settings for the given month. If no settings have
    // been saved yet for that month, returns a record with month=ym
    // and configured=false.
    BudgetSettings loadBudget(const std::string& userId,
                              const std::string& ym);
    void           saveBudget(const std::string& userId,
                              const BudgetSettings& budget);

    // --- Special Events ---------------------------------
    std::vector<SpecialEvent> loadEvents(const std::string& userId);
    void saveEvents(const std::string& userId,
                    const std::vector<SpecialEvent>& events);

    // --- Split Groups -----------------------------------
    // Uses multi-line block format for nested data
    std::vector<Group> loadGroups(const std::string& userId);
    void               saveGroups(const std::string& userId,
                                  const std::vector<Group>& groups);
}
