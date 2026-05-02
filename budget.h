#pragma once
// budget.h - Daily budget module
// New formula:
//   N = (expectedIncome - savings - fixedExpense) / daysInMonth
//
// k = remaining_pool / remaining_days
// where remaining_pool = monthlyDailyPool - dailyExpensesSpentSoFar
//
// Warnings:
//   - "Daily warning":   today's daily expenses > N
//   - "Monthly warning": k < 0.75 * N

#include "models.h"
#include <string>

namespace Budget {

    struct NResult {
        bool   configured = false; // true if user has filled out monthly form

        // Inputs (from BudgetSettings for the current month)
        double expectedIncome = 0.0;
        double savings        = 0.0;
        double fixedExpense   = 0.0;

        // Computed
        double monthlyPool = 0.0;   // = expectedIncome - savings - fixedExpense
        int    daysInMonth = 0;
        int    daysLeft    = 0;     // including today
        double n           = 0.0;   // monthlyPool / daysInMonth

        // Live tracking
        double dailySpentMonthToDate = 0.0; // sum of "daily" expenses in this month
        double dailySpentToday       = 0.0; // sum of "daily" expenses dated today
        double remainingPool         = 0.0; // monthlyPool - dailySpentMonthToDate
        double k                     = 0.0; // remainingPool / daysLeft
    };

    // Compute N and related values for the user's current month.
    NResult calcN(const std::string& userId);

    // Returns true if today's "daily" expenses exceed N.
    bool isDailyWarning(const NResult& r);

    // Returns true if k < 0.75 * N (and the user is configured).
    bool isMonthlyWarning(const NResult& r);

    // Show the monthly setup form and save the answers. Called by main.cpp
    // when the user logs in during a new month.
    void runMonthlySetup(const std::string& userId, const std::string& ym);

    // Enter the daily-budget submenu (main entry).
    void run(const std::string& userId);
}
