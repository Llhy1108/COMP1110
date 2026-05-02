#pragma once
// utils.h - Common utility functions
// Maintainer: can be maintained as a standalone module
// Includes: formatting, dates, UI input/output, ASCII charts

#include <string>
#include <vector>

namespace Utils {

    // --- String handling --------------------------------
    std::string genId();
    std::string trim(const std::string& s);
    std::vector<std::string> splitStr(const std::string& s, char delim);
    std::string joinStr(const std::vector<std::string>& v, char delim);

    // --- Dates ------------------------------------------
    std::string getToday();         // Returns "YYYY-MM-DD"
    std::string getCurrentMonth();  // Returns "YYYY-MM"
    int  getDaysLeft();             // Days remaining this month (incl. today)
    int  getDaysInMonth(const std::string& ym); // Total days in given month
    std::string getPreviousMonth(const std::string& ym); // "YYYY-MM" -> previous "YYYY-MM"
    std::string getDateNDaysAgo(int n); // n days before today, "YYYY-MM-DD"

    // --- Money formatting -------------------------------
    std::string fmtMoney(double amount); // "$1,234.56"

    // --- Terminal UI ------------------------------------
    void clearScreen();
    void pause();                         // Press Enter to continue
    void printLine(int width = 60, char c = '-');
    void printHeader(const std::string& title);
    void printMsg(const std::string& msg, bool isError = false);

    // --- User input -------------------------------------
    std::string getInput(const std::string& prompt);
    std::string getInputOptional(const std::string& prompt); // Allows empty
    double      getDoubleInput(const std::string& prompt);   // Must be >= 0
    int         getIntInput(const std::string& prompt);
    std::string getDateInput(const std::string& prompt,
                             bool allowFuture = false);   // Validates YYYY-MM-DD
    std::string getMonthInput(const std::string& prompt);    // Validates YYYY-MM

    // --- ASCII charts -----------------------------------
    void printProgressBar(double pct, int width = 30);
    void printBarChart(
        const std::vector<std::pair<std::string, double>>& rows,
        double maxVal,
        double totalForPercent = 0.0  // if > 0, append "(p%)" after each amount
    );
}
