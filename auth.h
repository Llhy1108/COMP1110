#pragma once
// auth.h - User authentication module
// Handles login, registration, and account management

#include "models.h"
#include <string>

namespace Auth {
    // Show login/register menu, returns the logged-in user.
    // Returns an empty User (id is empty) if the user chose to exit.
    User showAuthMenu();

    // Returns the currently logged-in user (call after showAuthMenu).
    // Login state is kept internally.
    bool isLoggedIn();
    User getCurrentUser();
}
