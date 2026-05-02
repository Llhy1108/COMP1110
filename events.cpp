// events.cpp - Special-event budget module implementation
// Special events are independent of the daily budget - they don't affect
// the day-to-day part of the N value calculation.

#include "events.h"
#include "storage.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

// --- Show event list ------------------------------------

static void showEvents(const string& uid) {
    auto evs = Storage::loadEvents(uid);
    if (evs.empty()) {
        cout << "  -- No special events --\n";
        return;
    }

    string today = Utils::getToday();
    Utils::printLine(80, '-');
    cout << "  " << left
         << setw(4)  << "No."
         << setw(20) << "Name"
         << setw(13) << "Start"
         << setw(13) << "End"
         << setw(12) << "Budget"
         << setw(12) << "Spent"
         << "Status\n";
    Utils::printLine(80, '-');

    int idx = 1;
    for (auto& e : evs) {
        string status;
        if (e.endDate < today)         status = "Ended";
        else if (e.startDate <= today) status = "Active";
        else                           status = "Upcoming";

        cout << "  " << left
             << setw(4)  << idx++
             << setw(20) << e.name
             << setw(13) << e.startDate
             << setw(13) << e.endDate
             << setw(12) << Utils::fmtMoney(e.budget)
             << setw(12) << Utils::fmtMoney(e.spent)
             << status << "\n";

        // Progress bar
        double pct = e.budget > 0
                     ? min(100.0, e.spent / e.budget * 100.0)
                     : 0.0;
        cout << "       ";
        Utils::printProgressBar(pct, 40);
        cout << "       Remaining: " << Utils::fmtMoney(e.budget - e.spent) << "\n\n";
    }
    Utils::printLine(80, '-');
}

// --- Add event ------------------------------------------

static void addEvent(const string& uid) {
    Utils::printHeader("Create New Special Event");

    string name  = Utils::getInput("Event name (e.g. Tokyo Trip)");
    string start = Utils::getDateInput("Start date", true);  // events can be future
    string end   = Utils::getDateInput("End date",   true);

    if (end < start) {
        Utils::printMsg("End date cannot be earlier than start date.", true);
        return;
    }

    double budget = Utils::getDoubleInput("Budget amount");
    if (budget <= 0) {
        Utils::printMsg("Budget amount must be greater than 0.", true);
        return;
    }

    string desc = Utils::getInputOptional("Description");

    SpecialEvent ev;
    ev.id          = Utils::genId();
    ev.name        = name;
    ev.startDate   = start;
    ev.endDate     = end;
    ev.budget      = budget;
    ev.spent       = 0.0;
    ev.description = desc;

    auto evs = Storage::loadEvents(uid);
    evs.push_back(ev);
    Storage::saveEvents(uid, evs);
    Utils::printMsg("Event created: " + name);
}

// --- Delete event ---------------------------------------

static void deleteEvent(const string& uid) {
    auto evs = Storage::loadEvents(uid);
    if (evs.empty()) {
        Utils::printMsg("No events to delete.", true);
        return;
    }
    int idx = Utils::getIntInput("Number to delete");
    if (idx < 1 || idx > (int)evs.size()) {
        Utils::printMsg("Invalid number.", true);
        return;
    }
    string name = evs[idx-1].name;
    evs.erase(evs.begin() + idx - 1);
    Storage::saveEvents(uid, evs);
    Utils::printMsg("Event deleted: " + name);
}

// --- View event detail (transactions linked to it) ------

static string catName(const string& uid, const string& catId) {
    auto cats = Storage::loadCategories(uid);
    for (auto& c : cats)
        if (c.id == catId) return c.name;
    return catId;
}

static void viewEventDetail(const string& uid) {
    auto evs = Storage::loadEvents(uid);
    if (evs.empty()) {
        Utils::printMsg("No events to view.", true);
        return;
    }
    int idx = Utils::getIntInput("Event number to view");
    if (idx < 1 || idx > (int)evs.size()) {
        Utils::printMsg("Invalid number.", true);
        return;
    }
    const SpecialEvent& ev = evs[idx - 1];

    Utils::printHeader("Event Detail - " + ev.name);

    cout << "  Date range  : " << ev.startDate << " to " << ev.endDate << "\n";
    cout << "  Budget      : " << Utils::fmtMoney(ev.budget) << "\n";
    cout << "  Spent       : " << Utils::fmtMoney(ev.spent)  << "\n";
    cout << "  Remaining   : " << Utils::fmtMoney(ev.budget - ev.spent) << "\n";
    if (!ev.description.empty())
        cout << "  Description : " << ev.description << "\n";

    double pct = ev.budget > 0
                 ? min(100.0, ev.spent / ev.budget * 100.0)
                 : 0.0;
    cout << "  Progress    : ";
    Utils::printProgressBar(pct);

    if (ev.spent > ev.budget && ev.budget > 0)
        cout << "  !! [EVENT OVERRUN] Spent exceeds budget by "
             << Utils::fmtMoney(ev.spent - ev.budget) << ".\n";
    cout << "\n";

    // Pull transactions tagged with this event
    auto txs = Storage::loadTransactions(uid);
    vector<Transaction> linked;
    for (auto& t : txs)
        if (t.eventId == ev.id)
            linked.push_back(t);

    sort(linked.begin(), linked.end(),
         [](const Transaction& a, const Transaction& b){ return a.date < b.date; });

    cout << "  -- Linked transactions (" << linked.size() << ") -----------\n";
    if (linked.empty()) {
        cout << "  -- No transactions tagged to this event yet --\n";
        return;
    }

    Utils::printLine(70, '-');
    cout << "  " << left
         << setw(12) << "Date"
         << setw(16) << "Category"
         << setw(12) << "Amount"
         << "Description\n";
    Utils::printLine(70, '-');
    for (auto& t : linked) {
        cout << "  " << left
             << setw(12) << t.date
             << setw(16) << catName(uid, t.category)
             << setw(12) << Utils::fmtMoney(t.amount)
             << t.description << "\n";
    }
    Utils::printLine(70, '-');
}

// --- Module entry ---------------------------------------

void Events::run(const string& uid) {
    while (true) {
        Utils::printHeader("Special Event Budgets");
        showEvents(uid);

        cout << "\n  [1] New event\n";
        cout << "  [2] Delete event\n";
        cout << "  [3] View event detail (transactions)\n";
        cout << "  [0] Back to main menu\n\n";

        string ch = Utils::getInput("Choice");
        if      (ch == "1") { addEvent(uid);       Utils::pause(); }
        else if (ch == "2") { deleteEvent(uid);    Utils::pause(); }
        else if (ch == "3") { viewEventDetail(uid); Utils::pause(); }
        else if (ch == "0") break;
        else { Utils::printMsg("Invalid option.", true); Utils::pause(); }
    }
}
