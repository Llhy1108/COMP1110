// split.cpp - Group expense splitting module
//
// Workflow:
//   1. Create a group / event, give it a name.
//   2. Type in the names of all participants (just names, not users).
//   3. For each shared expense:
//        - pick who paid
//        - tick who was involved (only those people owe a share)
//        - enter the amount and a description (the "why")
//        - choose how to split among the involved people:
//            * equal split
//            * by percentage
//            * by fixed amounts
//   4. At any time you can ask for the settlement plan.

#include "split.h"
#include "storage.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <set>
#include <cmath>

using namespace std;

// --- Settlement algorithm -------------------------------

vector<Settlement> Split::calcSettlements(const Group& g) {
    map<string, double> balance;
    for (auto& m : g.members)
        balance[m.id] = 0.0;

    for (auto& ex : g.expenses) {
        balance[ex.paidBy] += ex.amount;
        for (auto& s : ex.splits)
            balance[s.memberId] -= s.amount;
    }

    vector<pair<string, double>> cred, debt;
    for (auto& [id, amt] : balance) {
        if (amt > 0.01)  cred.push_back({id, amt});
        if (amt < -0.01) debt.push_back({id, -amt});
    }

    auto findName = [&](const string& id) -> string {
        for (auto& m : g.members)
            if (m.id == id) return m.name;
        return id;
    };

    vector<Settlement> results;
    size_t ci = 0, di = 0;
    while (ci < cred.size() && di < debt.size()) {
        double pay = min(cred[ci].second, debt[di].second);
        if (pay > 0.01) {
            Settlement s;
            s.fromId   = debt[di].first;
            s.fromName = findName(debt[di].first);
            s.toId     = cred[ci].first;
            s.toName   = findName(cred[ci].first);
            s.amount   = pay;
            results.push_back(s);
        }
        cred[ci].second -= pay;
        debt[di].second -= pay;
        if (cred[ci].second < 0.01) ci++;
        if (debt[di].second < 0.01) di++;
    }
    return results;
}

// --- Helpers --------------------------------------------

static string memberNameById(const Group& g, const string& mid) {
    for (auto& m : g.members)
        if (m.id == mid) return m.name;
    return mid;
}

// --- Create group ---------------------------------------

static void createGroup(const string& uid) {
    Utils::printHeader("Create New Event / Group");

    string name = Utils::getInput("Event name (e.g. Japan Trip)");

    Group g;
    g.id      = Utils::genId();
    g.name    = name;
    g.created = Utils::getToday();

    cout << "\n  Add participants (just type names, blank line to finish):\n";
    int counter = 1;
    while (true) {
        cout << "  Participant name (blank to finish): ";
        string pname; getline(cin, pname);
        pname = Utils::trim(pname);
        if (pname.empty()) break;

        // Reject duplicate names within the same group
        bool dup = false;
        for (auto& m : g.members)
            if (m.name == pname) { dup = true; break; }
        if (dup) { cout << "  '" << pname << "' is already in the list.\n"; continue; }

        GroupMember m;
        m.id   = "m" + to_string(counter++);
        m.name = pname;
        g.members.push_back(m);
        cout << "  Added: " << pname << "\n";
    }

    if (g.members.size() < 2) {
        Utils::printMsg("An event needs at least 2 participants.", true);
        return;
    }

    auto groups = Storage::loadGroups(uid);
    groups.push_back(g);
    Storage::saveGroups(uid, groups);
    Utils::printMsg("Event created: " + name);
}

// --- Add expense ----------------------------------------

static void addExpense(const string& uid, Group& g) {
    Utils::printHeader("Add Shared Expense - " + g.name);

    string desc   = Utils::getInput("Why (description)");
    double amount = Utils::getDoubleInput("Total amount");
    if (amount <= 0) { Utils::printMsg("Amount must be greater than 0.", true); return; }
    string date = Utils::getDateInput("Date");

    // Who paid?
    cout << "\n  Who paid?\n";
    for (size_t i = 0; i < g.members.size(); i++)
        cout << "  [" << i+1 << "] " << g.members[i].name << "\n";
    int payIdx = Utils::getIntInput("Select");
    if (payIdx < 1 || payIdx > (int)g.members.size()) {
        Utils::printMsg("Invalid option.", true); return;
    }
    string paidBy = g.members[payIdx-1].id;

    // Who was involved?
    cout << "\n  Who was involved? (type the numbers separated by space, "
         << "or 'all')\n";
    for (size_t i = 0; i < g.members.size(); i++)
        cout << "  [" << i+1 << "] " << g.members[i].name << "\n";
    cout << "  Select: ";
    string sel; getline(cin, sel);
    sel = Utils::trim(sel);

    vector<GroupMember> involved;
    if (sel == "all" || sel.empty()) {
        involved = g.members;
    } else {
        set<int> picked;
        auto tokens = Utils::splitStr(sel, ' ');
        for (auto& tok : tokens) {
            tok = Utils::trim(tok);
            if (tok.empty()) continue;
            try {
                int n = stoi(tok);
                if (n >= 1 && n <= (int)g.members.size())
                    picked.insert(n);
            } catch (...) {}
        }
        for (int n : picked) involved.push_back(g.members[n-1]);
    }
    if (involved.empty()) {
        Utils::printMsg("No one selected, cancelled.", true);
        return;
    }

    // How to split among the involved members?
    cout << "\n  Split method:\n";
    cout << "  [1] Equal split\n";
    cout << "  [2] By percentage\n";
    cout << "  [3] By fixed amount\n";
    string splitChoice = Utils::getInput("Select");

    vector<SplitEntry> splits;
    int n = (int)involved.size();

    if (splitChoice == "1") {
        double each = amount / n;
        for (auto& m : involved)
            splits.push_back({m.id, each});

    } else if (splitChoice == "2") {
        cout << "\n  Enter percentage for each involved member "
             << "(must total 100%):\n";
        double total = 0.0;
        for (auto& m : involved) {
            double pct = Utils::getDoubleInput(m.name + " (%)");
            splits.push_back({m.id, pct / 100.0 * amount});
            total += pct;
        }
        if (fabs(total - 100.0) > 0.1) {
            Utils::printMsg("Percentages don't total 100%, cancelled.", true);
            return;
        }

    } else if (splitChoice == "3") {
        cout << "\n  Enter amount for each involved member "
             << "(must equal total " << Utils::fmtMoney(amount) << "):\n";
        double total = 0.0;
        for (auto& m : involved) {
            double amt = Utils::getDoubleInput(m.name);
            splits.push_back({m.id, amt});
            total += amt;
        }
        if (fabs(total - amount) > 0.05) {
            Utils::printMsg("Amounts don't equal total, cancelled.", true);
            return;
        }
    } else {
        Utils::printMsg("Invalid option.", true); return;
    }

    GroupExpense ex;
    ex.id          = Utils::genId();
    ex.description = desc;
    ex.amount      = amount;
    ex.date        = date;
    ex.paidBy      = paidBy;
    ex.splits      = splits;

    g.expenses.push_back(ex);

    auto groups = Storage::loadGroups(uid);
    for (auto& gr : groups)
        if (gr.id == g.id) { gr = g; break; }
    Storage::saveGroups(uid, groups);
    Utils::printMsg("Expense recorded.");
}

// --- Group detail screen --------------------------------

static void showSettlement(const Group& g) {
    auto setts = Split::calcSettlements(g);
    cout << "\n  == Settlement Plan ==\n";
    if (setts.empty()) {
        cout << "  >> All settled, no transfers needed.\n";
    } else {
        for (auto& s : setts)
            cout << "  " << s.fromName
                 << " should pay " << s.toName
                 << " " << Utils::fmtMoney(s.amount) << "\n";
    }
}

static void showGroupDetail(const string& uid, Group& g) {
    while (true) {
        Utils::printHeader("Event: " + g.name);

        cout << "  Participants: ";
        for (size_t i = 0; i < g.members.size(); i++) {
            cout << g.members[i].name;
            if (i + 1 < g.members.size()) cout << ", ";
        }
        cout << "\n\n";

        if (g.expenses.empty()) {
            cout << "  -- No expense records yet --\n\n";
        } else {
            Utils::printLine(75, '-');
            cout << "  " << left
                 << setw(4)  << "No."
                 << setw(12) << "Date"
                 << setw(24) << "Description"
                 << setw(12) << "Amount"
                 << "Paid By\n";
            Utils::printLine(75, '-');

            int idx = 1;
            for (auto& ex : g.expenses) {
                cout << "  " << left
                     << setw(4)  << idx++
                     << setw(12) << ex.date
                     << setw(24) << ex.description
                     << setw(12) << Utils::fmtMoney(ex.amount)
                     << memberNameById(g, ex.paidBy) << "\n";
                cout << "       Involved: ";
                for (auto& s : ex.splits)
                    cout << memberNameById(g, s.memberId)
                         << "(" << Utils::fmtMoney(s.amount) << ")  ";
                cout << "\n";
            }
            Utils::printLine(75, '-');
        }

        cout << "\n  [1] Add expense\n";
        cout << "  [2] Delete expense\n";
        cout << "  [3] Show settlement plan\n";
        cout << "  [0] Back\n\n";

        string ch = Utils::getInput("Choice");
        if (ch == "1") {
            addExpense(uid, g);
            // Reload g from disk to pick up updates
            auto groups = Storage::loadGroups(uid);
            for (auto& gr : groups)
                if (gr.id == g.id) { g = gr; break; }
            Utils::pause();
        } else if (ch == "2") {
            if (g.expenses.empty()) {
                Utils::printMsg("No expenses to delete.", true);
            } else {
                int idx = Utils::getIntInput("Number to delete");
                if (idx >= 1 && idx <= (int)g.expenses.size()) {
                    g.expenses.erase(g.expenses.begin() + idx - 1);
                    auto groups = Storage::loadGroups(uid);
                    for (auto& gr : groups)
                        if (gr.id == g.id) { gr = g; break; }
                    Storage::saveGroups(uid, groups);
                    Utils::printMsg("Expense deleted.");
                } else {
                    Utils::printMsg("Invalid number.", true);
                }
            }
            Utils::pause();
        } else if (ch == "3") {
            showSettlement(g);
            Utils::pause();
        } else if (ch == "0") {
            break;
        }
    }
}

// --- Module entry ---------------------------------------

void Split::run(const string& uid) {
    while (true) {
        Utils::printHeader("Group Splitting");

        auto groups = Storage::loadGroups(uid);

        if (groups.empty()) {
            cout << "  -- No events yet --\n\n";
        } else {
            cout << "  No.  Event Name              Members  Expenses\n";
            Utils::printLine(55, '-');
            for (size_t i = 0; i < groups.size(); i++) {
                cout << "  [" << i+1 << "]   "
                     << left << setw(24) << groups[i].name
                     << setw(9) << groups[i].members.size()
                     << groups[i].expenses.size() << "\n";
            }
            Utils::printLine(55, '-');
        }

        cout << "\n";
        if (!groups.empty()) {
            cout << "  [1-" << groups.size() << "] Open an event\n";
            cout << "  [d] Delete event\n";
        }
        cout << "  [n] New event\n";
        cout << "  [0] Back to main menu\n\n";

        string ch = Utils::getInput("Choice");

        if (ch == "n") {
            createGroup(uid); Utils::pause();
        } else if (ch == "d") {
            if (groups.empty()) {
                Utils::printMsg("No events to delete.", true);
            } else {
                int idx = Utils::getIntInput("Number to delete");
                if (idx >= 1 && idx <= (int)groups.size()) {
                    string gid = groups[idx-1].id;
                    groups.erase(remove_if(groups.begin(), groups.end(),
                        [&](const Group& g){ return g.id == gid; }),
                        groups.end());
                    Storage::saveGroups(uid, groups);
                    Utils::printMsg("Event deleted.");
                } else {
                    Utils::printMsg("Invalid number.", true);
                }
            }
            Utils::pause();
        } else if (ch == "0") {
            break;
        } else {
            try {
                int idx = stoi(ch);
                if (idx >= 1 && idx <= (int)groups.size()) {
                    showGroupDetail(uid, groups[idx-1]);
                } else {
                    Utils::printMsg("Invalid option.", true); Utils::pause();
                }
            } catch (...) {
                Utils::printMsg("Invalid option.", true); Utils::pause();
            }
        }
    }
}
