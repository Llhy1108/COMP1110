// auth.cpp - User authentication module implementation

#include "auth.h"
#include "storage.h"
#include "utils.h"
#include <iostream>
#include <algorithm>

// Internal state
static User   g_currentUser;
static bool   g_loggedIn = false;

// --- Internal functions ---------------------------------

static bool doLogin() {
    std::cout << "\n  -- Login --\n";
    std::string username = Utils::getInput("Username");
    std::cout << "  Password: ";
    std::string password;
    std::getline(std::cin, password);

    auto users = Storage::loadUsers();
    auto it = std::find_if(users.begin(), users.end(),
        [&](const User& u) {
            return u.username == username && u.password == password;
        });

    if (it == users.end()) {
        Utils::printMsg("Invalid username or password.", true);
        return false;
    }
    g_currentUser = *it;
    g_loggedIn    = true;
    Utils::printMsg("Login successful! Welcome back, " + it->name + ".");
    return true;
}

static bool doRegister() {
    std::cout << "\n  -- Register New Account --\n";
    std::string username = Utils::getInput("Username (unique ID)");
    //Check if the username is empty
    if (username.empty()) {
        Utils::printMsg("Username cannot be empty.", true);
        return false;
    }
    // Check if username already exists
    auto users = Storage::loadUsers();
    for (auto& u : users) {
        if (u.username == username) {
            Utils::printMsg("Username is already taken, please choose another.", true);
            return false;
        }
    }

    std::string name  = Utils::getInput("Display Name");
    if (name.empty()) {
        Utils::printMsg("Display name cannot be empty.", true);
        return false;
    }
    std::string email = Utils::getInput("Email");
    if (email.empty() || email.find('@') == std::string::npos) {
        Utils::printMsg("Invalid email.", true);
        return false;
    }
    std::cout << "  Password: ";
    std::string password;
    std::getline(std::cin >> std::ws, password);
    if (password.empty()) {
        Utils::printMsg("Password cannot be empty.", true);
        return false;
    }

    User newUser;
    newUser.id       = Utils::genId();
    newUser.username = username;
    newUser.name     = name;
    newUser.email    = email;
    newUser.password = password;
    newUser.created  = Utils::getToday();

    users.push_back(newUser);
    Storage::saveUsers(users);

    g_currentUser = newUser;
    g_loggedIn    = true;
    Utils::printMsg("Registration successful! Welcome, " + name + ".");
    return true;
}

// --- Public interface -----------------------------------

User Auth::showAuthMenu() {
    while (true) {
        Utils::clearScreen();
        Utils::printHeader("BUDGETBUDDY - Personal Finance Terminal");

        std::cout << "  [1] Login\n";
        std::cout << "  [2] Register New Account\n";
        std::cout << "  [0] Exit Program\n\n";

        std::string choice = Utils::getInput("Please select");

        if (choice == "1") {
            if (doLogin()) return g_currentUser;
            Utils::pause();
        } else if (choice == "2") {
            if (doRegister()) return g_currentUser;
            Utils::pause();
        } else if (choice == "0") {
            return User{}; // Empty User signals exit
        } else {
            Utils::printMsg("Invalid option.", true);
            Utils::pause();
        }
    }
}

bool Auth::isLoggedIn() {
    return g_loggedIn;
}

User Auth::getCurrentUser() {
    return g_currentUser;
}
