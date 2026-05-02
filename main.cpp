// main.cpp - Program entry point and main navigation

#include "auth.h"
#include "bookkeeping.h"
#include "budget.h"
#include "split.h"
#include "report.h"
#include "storage.h"
#include "utils.h"
#include <iostream>
#include <algorithm>
#include <iomanip>

using namespace std;

// --- Dashboard ------------------------------------------

static void showDashboard(const User& user) {
    Utils::printHeader("Dashboard");
    cout << "  Welcome back, " << user.name << " (@" << user.username << ")\n\n";

    string ym = Utils::getCurrentMonth();
    auto txs = Storage::loadTransactions(user.id);

    // This month's totals (all expense kinds combined for display)
    double income = 0.0, expense = 0.0;
    for (auto& t : txs) {
        if (t.date.substr(0, 7) != ym) continue;
        if (t.type == "income")  income  += t.amount;
        if (t.type == "expense") expense += t.amount;
    }

    cout << "  -- This Month (" << ym << ") -----------------\n";
    cout << "  Income      : " << Utils::fmtMoney(income)           << "\n";
    cout << "  Expense     : " << Utils::fmtMoney(expense)          << "\n";
    cout << "  Net Balance : " << Utils::fmtMoney(income - expense) << "\n\n";

    // Budget snapshot
    auto r = Budget::calcN(user.id);
    if (r.configured) {
        cout << "  -- Daily Budget ----------------------------\n";
        cout << "  N (target/day)         : " << Utils::fmtMoney(r.n) << "\n";
        cout << "  k (remaining pool/day) : " << Utils::fmtMoney(r.k) << "\n";
        cout << "  Today's daily expenses : "
             << Utils::fmtMoney(r.dailySpentToday) << "\n";

        if (Budget::isDailyWarning(r))
            cout << "  !! [DAILY WARNING] You spent more than N today.\n";
        if (Budget::isMonthlyWarning(r))
            cout << "  !! [MONTHLY WARNING] k is below 75% of N.\n";
        if (!Budget::isDailyWarning(r) && !Budget::isMonthlyWarning(r))
            cout << "  >> Budget status looks healthy.\n";
        cout << "\n";
    } else {
        cout << "  >> Monthly plan for " << ym << " not set yet.\n";
        cout << "     Go to 'Daily Budget' to enter it.\n\n";
    }

    // Recent transactions
    auto sorted = txs;
    sort(sorted.begin(), sorted.end(), [](const Transaction& a, const Transaction& b){
        return a.date > b.date;
    });
    cout << "  -- Recent Transactions ---------------------\n";
    int shown = 0;
    for (auto& t : sorted) {
        if (shown++ >= 5) break;
        cout << "  " << t.date << "  "
             << (t.type == "income" ? "+" : "-")
             << Utils::fmtMoney(t.amount)
             << "  " << t.description << "\n";
    }
    if (sorted.empty()) cout << "  -- No records yet --\n";
    cout << "\n";
}

static void showSettings(const User& user) {
    Utils::printHeader("Account Settings");
    cout << "  Username      : @" << user.username << "\n";
    cout << "  Display Name  : "  << user.name     << "\n";
    cout << "  Email         : "  << user.email    << "\n";
    cout << "  Registered On : "  << user.created  << "\n";
    cout << "  User ID       : "  << user.id       << "\n\n";
    cout << "  (To edit account info, edit data/users.json directly.)\n";
    Utils::pause();
}

// --- Main menu ------------------------------------------

static void mainMenu(const User& user) {
    while (true) {
        Utils::clearScreen();
        showDashboard(user);

        cout << "  -- Main Menu -------------------------------\n";
        cout << "  [1] Bookkeeping (income/expense, events)\n";
        cout << "  [2] Daily Budget\n";
        cout << "  [3] Group Splitting\n";
        cout << "  [4] User Report\n";
        cout << "  [5] Account Settings\n";
        cout << "  [0] Log Out\n\n";

        string ch = Utils::getInput("Choice");
        Utils::clearScreen();

        if      (ch == "1") Bookkeeping::run(user.id);
        else if (ch == "2") Budget::run(user.id);
        else if (ch == "3") Split::run(user.id);
        else if (ch == "4") Report::run(user.id);
        else if (ch == "5") showSettings(user);
        else if (ch == "0") {
            Utils::printMsg("Logged out.");
            break;
        } else {
            Utils::printMsg("Invalid option.", true);
            Utils::pause();
        }
    }
}

// --- New-month check ------------------------------------

static void ensureCurrentMonthSetup(const User& user) {
    string ym = Utils::getCurrentMonth();
    BudgetSettings b = Storage::loadBudget(user.id, ym);
    if (b.configured) return;

    // First login of a new month: check whether last month finished
    // in the red (expenses exceeded income) and warn the user.
    string lastYm = Utils::getPreviousMonth(ym);
    auto txs = Storage::loadTransactions(user.id);
    double lastIncome = 0.0, lastExpense = 0.0;
    for (auto& t : txs) {
        if (t.date.substr(0, 7) != lastYm) continue;
        if (t.type == "income")  lastIncome  += t.amount;
        if (t.type == "expense") lastExpense += t.amount;
    }

    Utils::clearScreen();
    cout << "\n  >> No budget plan found for " << ym << ".\n";
    cout << "     Let's set it up now.\n\n";

    if (lastExpense > lastIncome && lastExpense > 0) {
        Utils::printLine(60, '-');
        cout << "  !! [LAST MONTH WARNING]\n";
        cout << "     In " << lastYm << " your total expense ("
             << Utils::fmtMoney(-lastExpense) << ")\n";
        cout << "     exceeded your total income ("
             << Utils::fmtMoney(lastIncome) << ").\n";
        cout << "     Net loss: "
             << Utils::fmtMoney(lastIncome - lastExpense) << "\n";
        cout << "     Consider setting a tighter plan for " << ym << ".\n";
        Utils::printLine(60, '-');
        cout << "\n";
    }

    Utils::pause();
    Budget::runMonthlySetup(user.id, ym);
}

// --- Program entry --------------------------------------

int main() {
    Storage::init();

    while (true) {
        User user = Auth::showAuthMenu();
        if (user.id.empty()) {
            cout << "\n  Goodbye!\n\n";
            break;
        }
        ensureCurrentMonthSetup(user);
        mainMenu(user);
    }

    return 0;
}
