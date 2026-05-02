// bookkeeping.cpp - Bookkeeping module implementation
// Features:
//   * View/add/delete transactions
//   * Manage custom categories
//   * Embedded "Special Events" submenu (moved from main menu)
//
// Each expense is tagged with a kind:
//   "daily" - regular variable spend; counts against the N pool
//   "fixed" - already declared in monthly plan; does NOT count
//   "event" - linked to a special event; does NOT count

#include "bookkeeping.h"
#include "events.h"
#include "storage.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <vector>

using namespace std;

// --- Internal helpers -----------------------------------

static string catName(const string& uid, const string& catId) {
    auto cats = Storage::loadCategories(uid);
    for (auto& c : cats)
        if (c.id == catId) return c.name;
    return catId;
}

static string kindLabel(const Transaction& t) {
    if (t.type == "income") return "-";
    if (t.expenseKind == "daily") return "daily";
    if (t.expenseKind == "fixed") return "fixed";
    if (t.expenseKind == "event") return "event";
    return "?";
}

static void printTxTable(const string& uid,
                          const vector<Transaction>& txs) {
    if (txs.empty()) {
        cout << "  -- No records --\n";
        return;
    }
    Utils::printLine(82, '-');
    cout << "  " << left
         << setw(4)  << "No."
         << setw(12) << "Date"
         << setw(8)  << "Type"
         << setw(8)  << "Kind"
         << setw(16) << "Category"
         << setw(12) << "Amount"
         << "Description\n";
    Utils::printLine(82, '-');

    int idx = 1;
    for (auto& t : txs) {
        string amtStr = (t.type == "income" ? "+" : "-") +
                        Utils::fmtMoney(t.amount);
        cout << "  " << left
             << setw(4)  << idx++
             << setw(12) << t.date
             << setw(8)  << (t.type == "income" ? "INC" : "EXP")
             << setw(8)  << kindLabel(t)
             << setw(16) << catName(uid, t.category)
             << setw(12) << amtStr
             << t.description << "\n";
    }
    Utils::printLine(82, '-');
}

static void alertCategorySpikeForNewRecord(const string& uid,
                                           const vector<Transaction>& txs,
                                           const Transaction& added) {
    string today = Utils::getToday();
    if (added.type != "expense" || added.date != today) return;

    map<string, double> todayByCat;
    map<string, double> historyByDate;

    for (auto& t : txs) {
        if (t.type != "expense" || t.category != added.category) continue;

        if (t.date == today)
            todayByCat[t.category] += t.amount;
        else
            historyByDate[t.date] += t.amount;
    }

    if (historyByDate.size() < 3) return;

    double total = 0.0;
    for (auto& [date, amt] : historyByDate)
        total += amt;

    double avg = total / historyByDate.size();
    double todayAmt = todayByCat[added.category];

    if (todayAmt > 2.0 * avg) {
        cout << "  !! [SPIKE ALERT] " << catName(uid, added.category)
             << " spending today (" << Utils::fmtMoney(todayAmt)
             << ") is significantly higher than your daily average ("
             << Utils::fmtMoney(avg) << ").\n";
    }
}

// --- Add transaction ------------------------------------

static void addTransaction(const string& uid) {
    Utils::printHeader("Add Transaction");

    cout << "  Type:\n  [1] Income\n  [2] Expense\n";
    string typeChoice = Utils::getInput("Select");
    string type = (typeChoice == "1") ? "income" : "expense";

    double amount = Utils::getDoubleInput("Amount");
    if (amount <= 0) { Utils::printMsg("Amount must be greater than 0.", true); return; }

    auto cats = Storage::loadCategories(uid);
    vector<Category> filtered;
    for (auto& c : cats)
        if (c.type == type || c.type == "both")
            filtered.push_back(c);

    cout << "\n  Categories:\n";
    for (size_t i = 0; i < filtered.size(); i++)
        cout << "  [" << i+1 << "] " << filtered[i].name << "\n";

    int catIdx = Utils::getIntInput("Select category number");
    if (catIdx < 1 || catIdx > (int)filtered.size()) {
        Utils::printMsg("Invalid number.", true); return;
    }

    string date = Utils::getDateInput("Date");
    string desc = Utils::getInputOptional("Description");

    string expenseKind;
    string eventId;

    if (type == "expense") {
        cout << "\n  Expense kind:\n";
        cout << "  [1] Daily expense (counts against your daily allowance)\n";
        cout << "  [2] Fixed expense (already in your monthly plan, does not count)\n";

        // Only offer event-link if there is at least one event
        auto evs = Storage::loadEvents(uid);
        bool offerEvent = !evs.empty();
        if (offerEvent)
            cout << "  [3] Special event expense (linked to an event)\n";

        string kindCh = Utils::getInput("Select kind");
        if (kindCh == "1") expenseKind = "daily";
        else if (kindCh == "2") expenseKind = "fixed";
        else if (kindCh == "3" && offerEvent) {
            expenseKind = "event";
            cout << "\n  Link to event:\n";
            for (size_t i = 0; i < evs.size(); i++)
                cout << "  [" << i+1 << "] " << evs[i].name
                     << " (" << evs[i].startDate
                     << " ~ " << evs[i].endDate << ")\n";
            int evChoice = Utils::getIntInput("Select");
            if (evChoice < 1 || evChoice > (int)evs.size()) {
                Utils::printMsg("Invalid event selection, cancelled.", true);
                return;
            }
            eventId = evs[evChoice-1].id;
            evs[evChoice-1].spent += amount;
            Storage::saveEvents(uid, evs);
        } else {
            Utils::printMsg("Invalid kind, cancelled.", true);
            return;
        }
    }

    Transaction tx;
    tx.id          = Utils::genId();
    tx.type        = type;
    tx.amount      = amount;
    tx.category    = filtered[catIdx-1].id;
    tx.date        = date;
    tx.description = desc;
    tx.eventId     = eventId;
    tx.expenseKind = expenseKind;

    auto txs = Storage::loadTransactions(uid);
    txs.push_back(tx);
    Storage::saveTransactions(uid, txs);
    Utils::printMsg("Record added.");
    alertCategorySpikeForNewRecord(uid, txs, tx);

    // After adding a daily expense, show the monthly warning if triggered.
    if (type == "expense" && expenseKind == "daily") {
        // We need to call Budget::calcN, but to avoid a circular header
        // dependency we inline the check by reloading the data.
        BudgetSettings b = Storage::loadBudget(uid, Utils::getCurrentMonth());
        if (b.configured) {
            string ym = Utils::getCurrentMonth();
            int dim = Utils::getDaysInMonth(ym);
            int daysLeft = Utils::getDaysLeft();
            double pool = max(0.0, b.expectedIncome - b.savings - b.fixedExpense);
            double n    = (dim > 0) ? pool / dim : 0.0;

            // Sum daily expenses this month
            double spent = 0.0, todaySpent = 0.0;
            string today = Utils::getToday();
            for (auto& t : txs) {
                if (t.type != "expense" || t.expenseKind != "daily") continue;
                if (t.date.substr(0,7) != ym) continue;
                spent += t.amount;
                if (t.date == today) todaySpent += t.amount;
            }
            double remPool = max(0.0, pool - spent);
            double k = (daysLeft > 0) ? remPool / daysLeft : 0.0;

            if (n > 0 && todaySpent > n)
                cout << "  !! [DAILY WARNING] Today's daily expenses ("
                     << Utils::fmtMoney(todaySpent) << ") exceed N ("
                     << Utils::fmtMoney(n) << ").\n";
            if (n > 0 && k < 0.75 * n)
                cout << "  !! [MONTHLY WARNING] k = "
                     << Utils::fmtMoney(k) << "/day is below 75% of N ("
                     << Utils::fmtMoney(n) << "/day).\n";
        }
    }
}

// --- Delete transaction (by id, regardless of view order) ---

static void deleteTransaction(const string& uid,
                               const vector<Transaction>& visible) {
    if (visible.empty()) { Utils::printMsg("No records to delete.", true); return; }
    int idx = Utils::getIntInput("Enter the number to delete");
    if (idx < 1 || idx > (int)visible.size()) {
        Utils::printMsg("Invalid number.", true); return;
    }
    const Transaction& target = visible[idx - 1];

    if (!target.eventId.empty() && target.type == "expense") {
        auto evs = Storage::loadEvents(uid);
        for (auto& e : evs)
            if (e.id == target.eventId) {
                e.spent = max(0.0, e.spent - target.amount);
                break;
            }
        Storage::saveEvents(uid, evs);
    }

    auto all = Storage::loadTransactions(uid);
    auto it = std::find_if(all.begin(), all.end(),
        [&](const Transaction& t){ return t.id == target.id; });
    if (it == all.end()) {
        Utils::printMsg("Could not find that record on disk.", true);
        return;
    }
    all.erase(it);
    Storage::saveTransactions(uid, all);
    Utils::printMsg("Record deleted.");
}

// --- Browse transactions (with filter) ------------------

static void browseTransactions(const string& uid) {
    Utils::printHeader("Transactions");

    auto txs = Storage::loadTransactions(uid);
    sort(txs.begin(), txs.end(), [](const Transaction& a, const Transaction& b){
        return a.date > b.date;
    });

    cout << "  You have " << txs.size() << " total record(s) saved.\n\n";

    cout << "  Filter (press Enter to skip each one):\n";
    cout << "  Type [income/expense/blank]: ";
    string fType; getline(cin, fType); fType = Utils::trim(fType);
    if (!fType.empty() && fType != "income" && fType != "expense") {
        cout << "  (ignoring invalid type filter '" << fType << "')\n";
        fType = "";
    }

    string fMonth = Utils::getInputOptional("Month (YYYY-MM)");
    bool monthOk = (fMonth.size() == 7 && fMonth[4] == '-');
    if (!fMonth.empty() && !monthOk) {
        cout << "  (ignoring invalid month filter '" << fMonth
             << "', expected YYYY-MM)\n";
        fMonth = "";
    }

    vector<Transaction> filtered;
    for (auto& t : txs) {
        if (!fType.empty() && t.type != fType) continue;
        if (!fMonth.empty() && t.date.substr(0, 7) != fMonth) continue;
        filtered.push_back(t);
    }

    cout << "\n  Showing " << filtered.size() << " of " << txs.size()
         << " record(s)";
    if (!fType.empty() || !fMonth.empty()) {
        cout << "  [filter:";
        if (!fType.empty())  cout << " type=" << fType;
        if (!fMonth.empty()) cout << " month=" << fMonth;
        cout << "]";
    }
    cout << "\n\n";

    printTxTable(uid, filtered);

    cout << "\n  [d] Delete record  [Enter] Back\n";
    cout << "  Choice: ";
    string ch; getline(cin, ch);
    if (Utils::trim(ch) == "d")
        deleteTransaction(uid, filtered);
}

// --- Manage categories ----------------------------------

static void manageCategories(const string& uid) {
    while (true) {
        Utils::printHeader("Category Management");
        auto cats = Storage::loadCategories(uid);

        cout << "  No.  Name              Type\n";
        Utils::printLine(40, '-');
        for (size_t i = 0; i < cats.size(); i++)
            cout << "  [" << setw(2) << i+1 << "] "
                 << left << setw(18) << cats[i].name
                 << cats[i].type << "\n";

        cout << "\n  [a] Add  [d] Delete  [0] Back\n";
        string ch = Utils::getInput("Choice");

        if (ch == "0") break;
        else if (ch == "a") {
            string name = Utils::getInput("Category name");
            cout << "  Type [income/expense/both]: ";
            string type; getline(cin, type);
            type = Utils::trim(type);
            if (type != "income" && type != "expense" && type != "both") {
                Utils::printMsg("Invalid type.", true);
            } else {
                cats.push_back({Utils::genId(), name, type});
                Storage::saveCategories(uid, cats);
                Utils::printMsg("Category added.");
            }
        } else if (ch == "d") {
            int idx = Utils::getIntInput("Number to delete");
            if (idx < 1 || idx > (int)cats.size()) {
                Utils::printMsg("Invalid number.", true);
            } else {
                cats.erase(cats.begin() + idx - 1);
                Storage::saveCategories(uid, cats);
                Utils::printMsg("Category deleted.");
            }
        }
        Utils::pause();
    }
}

// --- Module entry ---------------------------------------

void Bookkeeping::run(const string& uid) {
    while (true) {
        Utils::printHeader("Bookkeeping");
        cout << "  [1] View Records\n";
        cout << "  [2] Add Record\n";
        cout << "  [3] Manage Categories\n";
        cout << "  [4] Special Event Budgets\n";
        cout << "  [0] Back to Main Menu\n\n";

        string ch = Utils::getInput("Choice");
        if      (ch == "1") { browseTransactions(uid); Utils::pause(); }
        else if (ch == "2") { addTransaction(uid);     Utils::pause(); }
        else if (ch == "3") { manageCategories(uid);                   }
        else if (ch == "4") { Events::run(uid);                        }
        else if (ch == "0") break;
        else { Utils::printMsg("Invalid option.", true); Utils::pause(); }
    }
}
