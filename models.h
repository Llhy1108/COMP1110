#pragma once
// models.h - All data structure definitions
// This file is shared by all modules. Do not add logic here.

#include <string>
#include <vector>

// --- User ----------------------------------------------
struct User {
    std::string id;
    std::string username;   // Unique login name
    std::string name;       // Display name
    std::string email;
    std::string password;
    std::string created;    // YYYY-MM-DD
};

// --- Transaction ---------------------------------------
struct Transaction {
    std::string id;
    std::string type;        // "income" | "expense"
    std::string category;    // category id
    std::string date;        // YYYY-MM-DD
    std::string description;
    std::string eventId;     // Linked special event id, empty if none
    std::string expenseKind; // "daily" | "fixed" | "event" (only set for expenses)
    double      amount = 0.0;
};

// --- Category ------------------------------------------
struct Category {
    std::string id;
    std::string name;
    std::string type;  // "income" | "expense" | "both"
};

// --- Monthly Budget Settings ---------------------------
// One record per (user, month). Created the first time the user
// logs in during a new month.
struct BudgetSettings {
    std::string month;           // "YYYY-MM" - the month this record describes
    double      expectedIncome  = 0.0;
    double      savings         = 0.0;  // amount the user wants to set aside
    double      fixedExpense    = 0.0;  // recurring fixed costs (rent, etc.)
    bool        configured      = false; // true once the user has filled the form
};

// --- Special Event -------------------------------------
struct SpecialEvent {
    std::string id;
    std::string name;
    std::string startDate;   // YYYY-MM-DD
    std::string endDate;     // YYYY-MM-DD
    std::string description;
    double      budget = 0.0;
    double      spent  = 0.0;
};

// --- Split (Group Expense) -----------------------------
// Members are now just names typed by the user; they do NOT have to
// be registered BudgetBuddy accounts.
struct GroupMember {
    std::string id;     // local id within the group, e.g. "m1", "m2"
    std::string name;
};

struct SplitEntry {
    std::string memberId;
    double      amount = 0.0;
};

struct GroupExpense {
    std::string              id;
    std::string              description;  // why
    std::string              date;
    std::string              paidBy;       // member id
    double                   amount = 0.0;
    std::vector<SplitEntry>  splits;       // who is involved & how much each owes
};

struct Group {
    std::string                  id;
    std::string                  name;
    std::string                  created;
    std::vector<GroupMember>     members;
    std::vector<GroupExpense>    expenses;
};

// --- Settlement Result ---------------------------------
struct Settlement {
    std::string fromId, fromName;
    std::string toId,   toName;
    double      amount = 0.0;
};
