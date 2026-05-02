// budget.cpp - Daily budget module implementation
//
// Formula:
//   N             = (expectedIncome - savings - fixedExpense) / daysInMonth
//   remainingPool = monthlyPool - dailyExpensesSpentSoFar
//   k             = remainingPool / daysLeft
//
// Warnings:
//   * Daily warning   -> today's daily expenses > N
//   * Monthly warning -> k < 0.75 * N

#include "budget.h"
#include "storage.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

// --- Calculation ----------------------------------------

Budget::NResult Budget::calcN(const string& uid) {
    NResult r;
    string ym = Utils::getCurrentMonth();
    auto bgt  = Storage::loadBudget(uid, ym);

    r.configured    = bgt.configured;
    r.expectedIncome = bgt.expectedIncome;
    r.savings        = bgt.savings;
    r.fixedExpense   = bgt.fixedExpense;

    r.daysInMonth = Utils::getDaysInMonth(ym);
    r.daysLeft    = Utils::getDaysLeft();

    r.monthlyPool = max(0.0, r.expectedIncome - r.savings - r.fixedExpense);
    r.n = (r.daysInMonth > 0) ? (r.monthlyPool / r.daysInMonth) : 0.0;

    // Sum daily expenses for this month (and for today)
    string today = Utils::getToday();
    auto txs = Storage::loadTransactions(uid);
    for (auto& t : txs) {
        if (t.type != "expense") continue;
        if (t.expenseKind != "daily") continue; // ignore "fixed" and "event"
        if (t.date.substr(0, 7) != ym) continue;
        r.dailySpentMonthToDate += t.amount;
        if (t.date == today) r.dailySpentToday += t.amount;
    }

    r.remainingPool = max(0.0, r.monthlyPool - r.dailySpentMonthToDate);
    r.k = (r.daysLeft > 0) ? (r.remainingPool / r.daysLeft) : 0.0;
    return r;
}

bool Budget::isDailyWarning(const NResult& r) {
    return r.configured && r.n > 0 && r.dailySpentToday > r.n;
}

bool Budget::isMonthlyWarning(const NResult& r) {
    return r.configured && r.n > 0 && r.k < 0.75 * r.n;
}

// --- Monthly setup form ---------------------------------

void Budget::runMonthlySetup(const string& uid, const string& ym) {
    Utils::printHeader("New Month Setup - " + ym);
    cout << "  A new month has started. Please fill in your plan.\n\n";

    BudgetSettings b;
    b.month        = ym;
    b.expectedIncome = Utils::getDoubleInput("Expected income this month");
    b.savings        = Utils::getDoubleInput("Amount you want to save this month");
    b.fixedExpense   = Utils::getDoubleInput("Fixed expenses this month (rent etc.)");
    b.configured     = true;

    Storage::saveBudget(uid, b);

    // Show the resulting N right away so the user knows what to expect.
    int dim  = Utils::getDaysInMonth(ym);
    double pool = max(0.0, b.expectedIncome - b.savings - b.fixedExpense);
    double n    = (dim > 0) ? pool / dim : 0.0;

    cout << "\n  Daily allowance pool: " << Utils::fmtMoney(pool)
         << " across " << dim << " day(s)\n";
    cout << "  Daily expense target N = " << Utils::fmtMoney(n) << " / day\n\n";
    Utils::pause();
}

// --- Display panels -------------------------------------

static void showNPanel(const string& uid) {
    auto r = Budget::calcN(uid);

    if (!r.configured) {
        cout << "  No budget set for this month yet.\n";
        cout << "  Use [2] to enter your monthly plan.\n\n";
        return;
    }

    cout << "  --- Monthly Plan ---\n";
    cout << "  Expected income      : " << Utils::fmtMoney(r.expectedIncome) << "\n";
    cout << "  Savings goal         : " << Utils::fmtMoney(r.savings)        << "\n";
    cout << "  Fixed expenses       : " << Utils::fmtMoney(r.fixedExpense)   << "\n";
    cout << "  Daily allowance pool : " << Utils::fmtMoney(r.monthlyPool)
         << " (" << r.daysInMonth << " days)\n";
    cout << "  N (target per day)   : " << Utils::fmtMoney(r.n) << "\n\n";

    cout << "  --- Live Tracking ---\n";
    cout << "  Daily-expense spent (this month) : "
         << Utils::fmtMoney(r.dailySpentMonthToDate) << "\n";
    cout << "  Pool remaining                   : "
         << Utils::fmtMoney(r.remainingPool) << "\n";
    cout << "  Days left (including today)      : " << r.daysLeft << "\n";
    cout << "  k (remaining pool / days left)   : "
         << Utils::fmtMoney(r.k) << " / day\n\n";

    cout << "  --- Today ---\n";
    cout << "  Daily-expense spent today : "
         << Utils::fmtMoney(r.dailySpentToday) << "\n";

    if (r.n > 0) {
        double pct = min(100.0, r.dailySpentToday / r.n * 100.0);
        cout << "  Today's usage of N        : ";
        Utils::printProgressBar(pct);
    }

    cout << "\n";
    if (Budget::isDailyWarning(r))
        cout << "  !! [DAILY WARNING] You spent more than N today.\n";
    if (Budget::isMonthlyWarning(r))
        cout << "  !! [MONTHLY WARNING] k has fallen below 75% of N.\n";
    if (!Budget::isDailyWarning(r) && !Budget::isMonthlyWarning(r))
        cout << "  >> Budget status looks healthy.\n";
}

// --- Module entry ---------------------------------------

void Budget::run(const string& uid) {
    while (true) {
        Utils::printHeader("Daily Budget");
        showNPanel(uid);

        cout << "  [1] Re-enter this month's plan\n";
        cout << "  [0] Back to main menu\n\n";

        string ch = Utils::getInput("Choice");
        if (ch == "1") {
            runMonthlySetup(uid, Utils::getCurrentMonth());
        }
        else if (ch == "0") break;
        else { Utils::printMsg("Invalid option.", true); Utils::pause(); }
    }
}
