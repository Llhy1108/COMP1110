// report.cpp - Monthly report module implementation
// Output: income/expense summary, category breakdown,
//         budget usage, ASCII charts, recent-trend analysis.

#include "report.h"
#include "budget.h"
#include "storage.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <map>
#include <vector>

using namespace std;

// --- Helper: look up category name ----------------------

static string catName(const string& uid, const string& catId) {
    auto cats = Storage::loadCategories(uid);
    for (auto& c : cats)
        if (c.id == catId) return c.name;
    return catId;
}

static string lowerText(string s) {
    for (char& ch : s)
        ch = (char)tolower((unsigned char)ch);
    return s;
}

// --- Alerts ---------------------------------------------

static void alertCategorySpike(const string& uid, const vector<Transaction>& txs) {
    map<string, double> todayByCat;
    map<string, map<string, double>> dailyByCat;
    string today = Utils::getToday();

    for (auto& t : txs) {
        if (t.type != "expense") continue;

        dailyByCat[t.category][t.date] += t.amount;

        if (t.date == today)
            todayByCat[t.category] += t.amount;
    }

    bool found = false;
    for (auto& [cid, todayAmt] : todayByCat) {
        double total = 0.0;
        int historyDays = 0;
        for (auto& [date, amt] : dailyByCat[cid]) {
            if (date == today) continue;
            total += amt;
            historyDays++;
        }

        if (historyDays < 3) continue;

        double avg = total / historyDays;
        if (todayAmt > 2.0 * avg) {
            cout << "!! [SPIKE ALERT] " << catName(uid, cid)
                 << " spending today (" << Utils::fmtMoney(todayAmt)
                 << ") is significantly higher than your daily average ("
                 << Utils::fmtMoney(avg) << ").\n";
            found = true;
        }
    }

    if (!found)
        cout << ">> No category spikes detected.\n";
}

static void alertCategoryPercentage(const string& uid, const vector<Transaction>& monthly) {
    map<string, double> thresholds = {
        {"Food", 40.0},
        {"Transport", 30.0},
        {"Shopping", 35.0},
        {"Subscription", 20.0},
        {"Subscriptions", 20.0}
    };
    map<string, double> expByCat;
    double totalExpense = 0.0;

    for (auto& t : monthly) {
        if (t.type != "expense") continue;
        expByCat[t.category] += t.amount;
        totalExpense += t.amount;
    }

    bool found = false;
    if (totalExpense > 0.0) {
        for (auto& [cid, amt] : expByCat) {
            string name = catName(uid, cid);
            if (!thresholds.count(name)) continue;

            double pct = amt / totalExpense * 100.0;
            double threshold = thresholds[name];
            if (pct > threshold) {
                cout << "!! [PERCENTAGE ALERT] " << name
                     << " accounts for " << fixed << setprecision(1) << pct
                     << "% of total expenses, exceeding "
                     << fixed << setprecision(1) << threshold << "%.\n";
                found = true;
            }
        }
    }

    if (!found)
        cout << ">> No category percentage issues.\n";
}

static void alertDataQuality(const string& uid, const vector<Transaction>& monthly) {
    int badCount = 0;

    for (auto& t : monthly) {
        if (t.type != "expense") continue;

        string category = lowerText(catName(uid, t.category));
        bool weakCategory   = (category == "other" || category == "uncategorized");
        bool noDescription  = t.description.empty();

        // Only count records that fail BOTH checks: a generic category
        // alone or a missing description alone is not enough to flag.
        if (weakCategory && noDescription)
            badCount++;
    }

    if (badCount == 0) {
        cout << ">> Data quality looks good.\n";
        return;
    }

    cout << "!! [DATA WARNING] " << badCount
         << " transaction(s) are in 'Other'/'Uncategorized' AND have no description.\n";
}

// --- Monthly report body --------------------------------

static void showMonthlyReport(const string& uid, const string& ym) {
    Utils::printHeader("Monthly Report - " + ym);

    auto txs = Storage::loadTransactions(uid);
    auto bgt = Storage::loadBudget(uid, ym);

    // Filter to this month
    vector<Transaction> monthly;
    for (auto& t : txs)
        if (t.date.substr(0, 7) == ym)
            monthly.push_back(t);

    // Totals
    double totalIncome  = 0.0;
    double totalExpense = 0.0;
    for (auto& t : monthly) {
        if (t.type == "income")  totalIncome  += t.amount;
        if (t.type == "expense") totalExpense += t.amount;
    }
    double net = totalIncome - totalExpense;

    // Break expenses down by kind so the user can see where the money went
    double expDaily = 0.0, expFixed = 0.0, expEvent = 0.0;
    for (auto& t : monthly) {
        if (t.type != "expense") continue;
        if      (t.expenseKind == "daily") expDaily += t.amount;
        else if (t.expenseKind == "fixed") expFixed += t.amount;
        else if (t.expenseKind == "event") expEvent += t.amount;
        else                               expDaily += t.amount; // legacy
    }

    // -- Overview ---------------------------------------------
    // Expense rows are shown with a leading "-" so the direction
    // of money flow is immediately clear at a glance.
    cout << "  +======================================+\n";
    cout << "  |  Total Income   " << right << setw(16)
         << Utils::fmtMoney(totalIncome)  << "     |\n";
    cout << "  |  Total Expense  " << right << setw(16)
         << Utils::fmtMoney(-totalExpense) << "     |\n";
    cout << "  |    daily        " << right << setw(16)
         << Utils::fmtMoney(-expDaily)     << "     |\n";
    cout << "  |    fixed        " << right << setw(16)
         << Utils::fmtMoney(-expFixed)     << "     |\n";
    cout << "  |    event        " << right << setw(16)
         << Utils::fmtMoney(-expEvent)     << "     |\n";
    cout << "  |  Net Balance    " << right << setw(16)
         << Utils::fmtMoney(net)           << "     |\n";
    cout << "  |  # Transactions " << right << setw(16)
         << monthly.size()                 << "     |\n";
    cout << "  +======================================+\n\n";

    alertCategorySpike(uid, txs);
    alertCategoryPercentage(uid, monthly);
    alertDataQuality(uid, monthly);
    cout << "\n";

    // -- Daily-pool usage (counts only "daily" expenses) -----
    if (bgt.configured) {
        double pool = max(0.0, bgt.expectedIncome - bgt.savings - bgt.fixedExpense);

        if (pool > 0) {
            double pct = min(100.0, expDaily / pool * 100.0);
            cout << "  Daily-pool usage (" << Utils::fmtMoney(expDaily)
                 << " / " << Utils::fmtMoney(pool) << "):\n";
            Utils::printProgressBar(pct);
            cout << "  Pool remaining: "
                 << Utils::fmtMoney(max(0.0, pool - expDaily)) << "\n\n";
        }
    }

    // -- Expense by category (bar + amount + percentage) -----
    map<string, double> expByCat, incByCat;
    for (auto& t : monthly) {
        if (t.type == "expense") expByCat[t.category] += t.amount;
        if (t.type == "income")  incByCat[t.category] += t.amount;
    }

    if (!expByCat.empty()) {
        cout << "  -- Expense by Category --------------------\n";
        vector<pair<string, double>> rows;
        double maxVal = 0.0;
        for (auto& [cid, amt] : expByCat) {
            rows.push_back({catName(uid, cid), amt});
            maxVal = max(maxVal, amt);
        }
        sort(rows.begin(), rows.end(),
             [](auto& a, auto& b){ return b.second < a.second; });
        Utils::printBarChart(rows, maxVal, totalExpense);
        cout << "\n";
    }

    if (!incByCat.empty()) {
        cout << "  -- Income by Category ---------------------\n";
        vector<pair<string, double>> rows;
        double maxVal = 0.0;
        for (auto& [cid, amt] : incByCat) {
            rows.push_back({catName(uid, cid), amt});
            maxVal = max(maxVal, amt);
        }
        sort(rows.begin(), rows.end(),
             [](auto& a, auto& b){ return b.second < a.second; });
        Utils::printBarChart(rows, maxVal, totalIncome);
        cout << "\n";
    }
}

// --- Daily-spending chart for any month -----------------
// Moved here from budget.cpp; works for any past or current month.

static void showDailyChart(const string& uid, const string& ym) {
    Utils::printHeader("Daily Spending - " + ym + " (daily expenses only)");

    auto txs = Storage::loadTransactions(uid);
    auto bgt = Storage::loadBudget(uid, ym);
    int  dim = Utils::getDaysInMonth(ym);

    double pool = max(0.0, bgt.expectedIncome - bgt.savings - bgt.fixedExpense);
    double n    = (bgt.configured && dim > 0) ? pool / dim : 0.0;

    vector<double> daily(dim + 1, 0.0);
    double maxVal = (n > 0) ? n : 1.0;
    for (auto& t : txs) {
        if (t.type != "expense") continue;
        if (t.expenseKind != "daily") continue;
        if (t.date.substr(0, 7) != ym) continue;
        try {
            int day = stoi(t.date.substr(8, 2));
            if (day >= 1 && day <= dim) {
                daily[day] += t.amount;
                maxVal = max(maxVal, daily[day]);
            }
        } catch (...) {}
    }

    const int BAR_W = 25;
    cout << "  Day | Amount        | Chart\n";
    Utils::printLine(60, '-');
    for (int d = 1; d <= dim; d++) {
        double v = daily[d];
        int w = (int)(v / maxVal * BAR_W);
        cout << "  " << setw(2) << d << "  | "
             << left << setw(13) << Utils::fmtMoney(v) << "| ";
        for (int i = 0; i < w; i++)
            cout << (v > n && n > 0 ? "!" : "#");
        cout << "\n";
    }
    if (n > 0)
        cout << "\n  N = " << Utils::fmtMoney(n) << "/day  (! = over N)\n";
    else
        cout << "\n  (No budget plan set for " << ym
             << ", so no N target line is drawn.)\n";
}

// --- Recent-window analysis (last N days) ---------------

static void showRecentWindow(const string& uid, int days) {
    Utils::printHeader("Last " + to_string(days) + " Days");

    string today    = Utils::getToday();
    string startCur = Utils::getDateNDaysAgo(days - 1);   // includes today

    auto txs = Storage::loadTransactions(uid);

    double curExp = 0.0, curInc = 0.0;
    map<string, double> curExpByCat;
    map<string, double> dayExp;          // for mini-chart of current window
    int activeDays = 0;
    map<string, bool> seenDays;

    for (auto& t : txs) {
        if (t.date < startCur || t.date > today) continue;

        if (t.type == "expense") {
            curExp += t.amount;
            curExpByCat[t.category] += t.amount;
            dayExp[t.date] += t.amount;
            if (!seenDays[t.date]) { activeDays++; seenDays[t.date] = true; }
        } else if (t.type == "income") {
            curInc += t.amount;
        }
    }

    double curAvg = curExp / days;

    cout << "  Window : " << startCur << " to " << today
         << "  (" << days << " days)\n\n";

    cout << "  Total expense        : " << Utils::fmtMoney(-curExp)         << "\n";
    cout << "  Total income         : " << Utils::fmtMoney(curInc)          << "\n";
    cout << "  Net                  : " << Utils::fmtMoney(curInc - curExp) << "\n";
    cout << "  Daily average expense: " << Utils::fmtMoney(curAvg)          << "\n";
    cout << "  Days with spending   : " << activeDays << " / " << days      << "\n\n";

    // Per-day mini chart - shows the spending trend across the window
    if (curExp > 0.0) {
        cout << "  -- Daily spending in window ---------------\n";
        double maxDay = 0.0;
        for (int i = days - 1; i >= 0; i--) {
            string d = Utils::getDateNDaysAgo(i);
            maxDay = max(maxDay, dayExp[d]);
        }
        if (maxDay <= 0.0) maxDay = 1.0;

        const int W = 22;
        for (int i = days - 1; i >= 0; i--) {
            string d = Utils::getDateNDaysAgo(i);
            double v = dayExp.count(d) ? dayExp[d] : 0.0;
            int    w = (int)(v / maxDay * W);
            cout << "  " << d << " | ";
            for (int j = 0; j < w; j++) cout << "#";
            for (int j = w; j < W; j++) cout << " ";
            cout << "  " << Utils::fmtMoney(v) << "\n";
        }
        cout << "\n";
    }

    // Top categories in current window
    if (!curExpByCat.empty()) {
        cout << "  -- Top spending categories ----------------\n";
        vector<pair<string, double>> rows;
        double maxVal = 0.0;
        for (auto& [cid, amt] : curExpByCat) {
            rows.push_back({catName(uid, cid), amt});
            maxVal = max(maxVal, amt);
        }
        sort(rows.begin(), rows.end(),
             [](auto& a, auto& b){ return b.second < a.second; });
        if (rows.size() > 5) rows.resize(5);
        Utils::printBarChart(rows, maxVal, curExp);
    }
}

// --- Module entry ---------------------------------------

void Report::run(const string& uid) {
    while (true) {
        Utils::printHeader("Monthly Report");
        cout << "  [1] Monthly summary (pick a month)\n";
        cout << "  [2] Daily spending chart (pick a month)\n";
        cout << "  [3] Last 7 days analysis\n";
        cout << "  [4] Last 30 days analysis\n";
        cout << "  [0] Back to main menu\n\n";

        string ch = Utils::getInput("Choice");

        if (ch == "1") {
            string ym = Utils::getMonthInput("Select month");
            showMonthlyReport(uid, ym);
            Utils::pause();
        } else if (ch == "2") {
            string ym = Utils::getMonthInput("Select month");
            showDailyChart(uid, ym);
            Utils::pause();
        } else if (ch == "3") {
            showRecentWindow(uid, 7);
            Utils::pause();
        } else if (ch == "4") {
            showRecentWindow(uid, 30);
            Utils::pause();
        } else if (ch == "0") {
            break;
        } else {
            Utils::printMsg("Invalid option.", true);
            Utils::pause();
        }
    }
}
