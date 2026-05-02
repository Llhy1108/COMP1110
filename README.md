# BudgetBuddy – Personal Budget and Spending Assistant

## 1. Introduction

BudgetBuddy is a command-line personal finance management system implemented in C++17.

This project models everyday spending as a structured computational problem. It allows users to record transactions, analyze spending patterns, and detect overspending through rule-based alerts.

The system is designed to answer:
- Where did my money go?
- Am I overspending?
- Which categories are problematic?

---

## 2. Programming Language and Environment

- Language: C++17
- Platform: Windows / macOS / Linux
- Compiler: g++ (or any C++17-compatible compiler)
- External libraries: None

---

## 3. Compilation and Execution

### Compile
g++ -std=c++17 *.cpp -o budgetbuddy.exe

### Run
./budgetbuddy.exe

On first run, the program will automatically create a `data/` directory to store user data.

---

## 4. Project Structure

budgetbuddy/
├── main.cpp
├── auth.cpp/.h
├── bookkeeping.cpp/.h
├── budget.cpp/.h
├── events.cpp/.h
├── split.cpp/.h
├── report.cpp/.h
├── storage.cpp/.h
├── utils.cpp/.h
├── models.h
└── data/

---

## 5. Data Model

### Transaction
Each transaction contains:
- date (YYYY-MM-DD)
- amount
- category
- description
- type (income / expense)
- expenseKind (daily / fixed / event)

### Budget Settings
- expectedIncome
- savings
- fixedExpense

### Special Event
- name, date range
- budget and tracked spending

---

## 6. Core Functionalities

### 6.1 Bookkeeping
- Add income and expense records
- Categorize transactions
- View transaction history

### 6.2 Daily Budget System

The system calculates:

N = (expectedIncome - savings - fixedExpense) / daysInMonth  
k = remainingPool / daysLeft  

Only **daily expenses** are counted toward budget tracking.

---

### 6.3 Alerts

1. Daily Warning  
   Trigger: today’s spending > N  

2. Monthly Warning  
   Trigger: k < 0.75 × N  

3. Category Spike Alert  
   Trigger: today’s spending > 2× historical average  

4. Category Percentage Alert  
   Trigger: category exceeds threshold  

5. Data Quality Warning  
   Trigger: uncategorized AND missing description  

---

### 6.4 Reports

- Total income and expense
- Net balance
- Category breakdown (sorted by spending, descending)
- Top-3 spending categories
- Expense type breakdown (daily / fixed / event)
- visualization (charts)
- Spending trends (last 7 days / last 30 days)

---

### 6.5 Special Events

- Independent budgeting system
- Event expenses do not affect daily budget

---

### 6.6 Group Expense Splitting

- Create groups
- Add shared expenses
- Compute optimized settlement plan

---

## 7. Input Validation and Error Handling

The system handles:
- Invalid numeric input
- Invalid date format
- Invalid category selection
- Empty required fields
- Invalid menu choices
- Missing or empty data files

---

## 8. Sample Test Cases

### TC1: Daily Overspend
Input: daily spending > N  
Expected: DAILY WARNING  

### TC2: Monthly Overspend
Input: consistent overspending  
Expected: MONTHLY WARNING  

### TC3: Category Spike
Input: today > 2× average  
Expected: SPIKE ALERT  

### TC4: Category Percentage
Input: category exceeds threshold  
Expected: PERCENTAGE ALERT  

### TC5: Data Quality
Input: uncategorized + empty description  
Expected: DATA WARNING  

### TC6: Event Budget
Input: event expenses  
Expected: tracked separately  

### TC7: Group Splitting
Input: shared expenses  
Expected: correct settlement plan  

---

## 9. Case Studies
-Unzip Case Test Data and move into "Data" file
-For the expected outcome of each case study, run the program and login with corresponding username and password
e.g. Case Study 1 login as Username "1" and passward "1" etc. 
### Case Study 1: Student Daily Budget
Controls daily spending using alerts.  
Example: Budget = 6000 → [DAILY WARNING] triggered when spending > N

### Case Study 2: Subscription Monitoring
Identifies dominant spending categories.  
Example: Output shows top spending category exceeds threshold

### Case Study 3: Shared Expenses
Minimizes settlement transactions.  
Example: Settlement → B → A: 487.5, C → A: 337.5

---

## 10. Design Trade-offs

| Decision | Trade-off |
|--------|--------|
| Manual input | Simple vs error-prone |
| Daily budget model | Clear control vs rigid |
| JSON storage | Easy vs not scalable |
| Rule-based alerts | Explainable vs limited |
| Category system | Simple vs less detailed |

---

## 11. Limitations

- Passwords stored in plain text
- No automatic data import
- No predictive analytics
- Manual categorization required
- Terminal interface only

---

## 12. Future Improvements

- Improve categorization system
- Add visualization tools
- Introduce anomaly detection

---

## 13. AI Usage Disclosure

AI tools (e.g., ChatGPT) were used for:
- code structuring
- debugging
- documentation refinement

All outputs were manually reviewed and tested.
