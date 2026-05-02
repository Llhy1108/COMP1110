// utils.cpp - Common utility functions implementation

#include "utils.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <random>
#include <algorithm>
#include <cctype>
#include <limits>

// --- String handling ------------------------------------

std::string Utils::genId() {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<> dist(0, 35);
    const char chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string id;
    for (int i = 0; i < 8; i++)
        id += chars[dist(rng)];
    return id;
}

std::string Utils::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::vector<std::string> Utils::splitStr(const std::string& s, char delim) {
    // Walks the string manually so that empty trailing fields are kept.
    // (std::getline drops a trailing empty token, which corrupts our
    //  pipe-separated records when the last field is empty.)
    std::vector<std::string> result;
    std::string token;
    for (char c : s) {
        if (c == delim) {
            result.push_back(token);
            token.clear();
        } else {
            token += c;
        }
    }
    result.push_back(token);
    return result;
}

std::string Utils::joinStr(const std::vector<std::string>& v, char delim) {
    std::string result;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) result += delim;
        result += v[i];
    }
    return result;
}

// --- Dates ----------------------------------------------

std::string Utils::getToday() {
    time_t now = time(nullptr);
    now += 86400 * 0;
    tm* t = localtime(&now);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", t);
    return buf;
}

std::string Utils::getCurrentMonth() {
    time_t now = time(nullptr);
    tm* t = localtime(&now);
    char buf[8];
    strftime(buf, sizeof(buf), "%Y-%m", t);
    return buf;
}

int Utils::getDaysLeft() {
    time_t now = time(nullptr);
    tm* t = localtime(&now);
    int year  = t->tm_year + 1900;
    int month = t->tm_mon + 1;
    int today = t->tm_mday;
    int dim   = getDaysInMonth(std::to_string(year) + "-" +
                               (month < 10 ? "0" : "") + std::to_string(month));
    return dim - today + 1;
}

int Utils::getDaysInMonth(const std::string& ym) {
    auto parts = splitStr(ym, '-');
    if (parts.size() < 2) return 30;
    int year  = std::stoi(parts[0]);
    int month = std::stoi(parts[1]);
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

std::string Utils::getPreviousMonth(const std::string& ym) {
    auto parts = splitStr(ym, '-');
    if (parts.size() < 2) return ym;
    int year, month;
    try {
        year  = std::stoi(parts[0]);
        month = std::stoi(parts[1]);
    } catch (...) { return ym; }
    month--;
    if (month < 1) { month = 12; year--; }
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d", year, month);
    return std::string(buf);
}

std::string Utils::getDateNDaysAgo(int n) {
    time_t now = time(nullptr) - (time_t)n * 86400;
    tm* t = localtime(&now);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", t);
    return std::string(buf);
}

// --- Money formatting -----------------------------------

std::string Utils::fmtMoney(double amount) {
    bool neg = amount < 0;
    if (neg) amount = -amount;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << amount;
    std::string s = oss.str();

    // Insert thousands separators
    size_t dot = s.find('.');
    int insertAt = (int)dot - 3;
    while (insertAt > 0) {
        s.insert(insertAt, ",");
        insertAt -= 3;
    }
    return (neg ? "-$" : "$") + s;
}

// --- Terminal UI ----------------------------------------

void Utils::clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void Utils::pause() {
    std::cout << "\n  Press Enter to continue...";
    std::cin.get();
}

void Utils::printLine(int width, char c) {
    std::cout << "  " << std::string(width, c) << "\n";
}

void Utils::printHeader(const std::string& title) {
    std::cout << "\n";
    printLine(60, '=');
    int pad = std::max(0, (int)(58 - (int)title.size()) / 2);
    std::cout << "  " << std::string(pad, ' ') << title << "\n";
    printLine(60, '=');
    std::cout << "\n";
}

void Utils::printMsg(const std::string& msg, bool isError) {
    if (isError)
        std::cout << "\n  [ERROR] " << msg << "\n\n";
    else
        std::cout << "\n  [OK]    " << msg << "\n\n";
}

// --- User input -----------------------------------------

std::string Utils::getInput(const std::string& prompt) {
    while (true) {
        std::cout << "  " << prompt << ": ";
        std::string val;
        std::getline(std::cin, val);
        val = trim(val);
        // Remove '|' and ';' to keep storage format safe
        val.erase(std::remove(val.begin(), val.end(), '|'), val.end());
        val.erase(std::remove(val.begin(), val.end(), ';'), val.end());
        if (!val.empty()) return val;
        std::cout << "  This field cannot be empty, please try again.\n";
    }
}

std::string Utils::getInputOptional(const std::string& prompt) {
    std::cout << "  " << prompt << " (optional): ";
    std::string val;
    std::getline(std::cin, val);
    val = trim(val);
    val.erase(std::remove(val.begin(), val.end(), '|'), val.end());
    val.erase(std::remove(val.begin(), val.end(), ';'), val.end());
    return val;
}

double Utils::getDoubleInput(const std::string& prompt) {
    while (true) {
        std::cout << "  " << prompt << " ($): ";
        std::string s;
        std::getline(std::cin, s);
        s = trim(s);
        try {
            double v = std::stod(s);
            if (v >= 0) return v;
        } catch (...) {}
        std::cout << "  Please enter a valid non-negative number.\n";
    }
}

int Utils::getIntInput(const std::string& prompt) {
    while (true) {
        std::cout << "  " << prompt << ": ";
        std::string s;
        std::getline(std::cin, s);
        s = trim(s);
        try { return std::stoi(s); }
        catch (...) { std::cout << "  Please enter an integer.\n"; }
    }
}

std::string Utils::getDateInput(const std::string& prompt, bool allowFuture) {
    while (true) {
        std::cout << "  " << prompt
                  << " (YYYYMMDD, e.g. 20260427, blank = today): ";
        std::string s;
        std::getline(std::cin, s);
        s = trim(s);

        if (s.empty()) return getToday();

        if (s.size() != 8 ||
            !std::all_of(s.begin(), s.end(),
                [](char c){ return std::isdigit((unsigned char)c); })) {
            std::cout << "  Invalid format. Type 8 digits, e.g. 20260427.\n";
            continue;
        }

        int y, m, d;
        try {
            y = std::stoi(s.substr(0, 4));
            m = std::stoi(s.substr(4, 2));
            d = std::stoi(s.substr(6, 2));
        } catch (...) {
            std::cout << "  Could not parse the date. Try again.\n";
            continue;
        }
        if (y < 1900 || y > 2999) {
            std::cout << "  Year " << y << " out of range (1900-2999).\n";
            continue;
        }
        if (m < 1 || m > 12) {
            std::cout << "  Month " << m << " is invalid.\n";
            continue;
        }
        std::string ym = s.substr(0, 4) + "-" + s.substr(4, 2);
        int maxDay = getDaysInMonth(ym);
        if (d < 1 || d > maxDay) {
            std::cout << "  Day " << d << " is invalid for " << ym
                      << " (this month has " << maxDay << " days).\n";
            continue;
        }
        std::string parsed = s.substr(0, 4) + "-" + s.substr(4, 2)
                           + "-" + s.substr(6, 2);

        if (!allowFuture && parsed > getToday()) {
            std::cout << "  Future dates are not allowed (today is "
                      << getToday() << ").\n";
            continue;
        }
        return parsed;
    }
}

std::string Utils::getMonthInput(const std::string& prompt) {
    while (true) {
        std::cout << "  " << prompt << " (YYYY-MM, blank = current month): ";
        std::string s;
        std::getline(std::cin, s);
        s = trim(s);
        if (s.empty()) return getCurrentMonth();
        if (s.size() == 7 && s[4] == '-') {
            try {
                int m = std::stoi(s.substr(5, 2));
                if (m >= 1 && m <= 12) return s;
            } catch (...) {}
        }
        std::cout << "  Invalid format, please use YYYY-MM.\n";
    }
}

// --- ASCII charts ---------------------------------------

void Utils::printProgressBar(double pct, int width) {
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    int filled = (int)(pct / 100.0 * width);

    std::cout << "  [";
    for (int i = 0; i < width; i++)
        std::cout << (i < filled ? "#" : "-");
    std::cout << "] " << std::fixed << std::setprecision(1) << pct << "%\n";
}

void Utils::printBarChart(
    const std::vector<std::pair<std::string, double>>& rows,
    double maxVal,
    double totalForPercent
) {
    const int BAR_W = 28;
    for (auto& [label, val] : rows) {
        int w = (maxVal > 0) ? (int)(val / maxVal * BAR_W) : 0;
        // Truncate label to 18 chars
        std::string lbl = label.size() > 18 ? label.substr(0, 17) + "~" : label;
        std::cout << "  " << std::left << std::setw(18) << lbl << " | ";
        for (int i = 0; i < w; i++)        std::cout << "#";
        for (int i = w; i < BAR_W; i++)    std::cout << " ";
        std::cout << "  " << std::left << std::setw(11) << fmtMoney(val);
        if (totalForPercent > 0) {
            double pct = val / totalForPercent * 100.0;
            std::cout << " (" << std::fixed << std::setprecision(1) << pct << "%)";
        }
        std::cout << "\n";
    }
}
