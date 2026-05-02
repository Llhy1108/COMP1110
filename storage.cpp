// storage.cpp - File I/O implementation
// Data dir: ./data/
// Primary format: JSON files.
// Legacy pipe-delimited .txt files are still readable for migration.

#include "storage.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <map>
#include <cctype>
#include <cstdlib>
#include <sys/stat.h>

// --- Internal helpers -----------------------------------

static std::string dataPath(const std::string& filename) {
    return "data/" + filename;
}

static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return static_cast<bool>(f);
}

static void ensureDir() {
#ifdef _WIN32
    mkdir("data");
#else
    mkdir("data", 0755);
#endif
}

// Read all non-empty, non-comment lines from a file
static std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream f(path);
    if (!f.is_open()) return lines;
    std::string line;
    while (std::getline(f, line)) {
        line = Utils::trim(line);
        if (!line.empty() && line[0] != '#')
            lines.push_back(line);
    }
    return lines;
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::map<std::string, JsonValue> objectValue;
};

static JsonValue makeNull() {
    return JsonValue();
}

static JsonValue makeBool(bool value) {
    JsonValue json;
    json.type = JsonValue::Type::Bool;
    json.boolValue = value;
    return json;
}

static JsonValue makeNumber(double value) {
    JsonValue json;
    json.type = JsonValue::Type::Number;
    json.numberValue = value;
    return json;
}

static JsonValue makeString(const std::string& value) {
    JsonValue json;
    json.type = JsonValue::Type::String;
    json.stringValue = value;
    return json;
}

static JsonValue makeArray(const std::vector<JsonValue>& values = {}) {
    JsonValue json;
    json.type = JsonValue::Type::Array;
    json.arrayValue = values;
    return json;
}

static JsonValue makeObject(const std::vector<std::pair<std::string, JsonValue>>& fields = {}) {
    JsonValue json;
    json.type = JsonValue::Type::Object;
    for (auto& field : fields)
        json.objectValue[field.first] = field.second;
    return json;
}

static std::string hexByte(unsigned char c) {
    const char* digits = "0123456789ABCDEF";
    std::string out(2, '0');
    out[0] = digits[(c >> 4) & 0x0F];
    out[1] = digits[c & 0x0F];
    return out;
}

static void appendUtf8(std::string& out, unsigned int codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

static std::string escapeJsonString(const std::string& input) {
    std::ostringstream out;
    for (unsigned char c : input) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << "\\u00" << hexByte(c);
                else out << static_cast<char>(c);
                break;
        }
    }
    return out.str();
}

static std::string indentString(int level) {
    return std::string(level * 2, ' ');
}

static std::string stringifyJson(const JsonValue& value, int level = 0);

static std::string stringifyArray(const std::vector<JsonValue>& values, int level) {
    if (values.empty()) return "[]";
    std::ostringstream out;
    out << "[\n";
    for (size_t i = 0; i < values.size(); ++i) {
        out << indentString(level + 1) << stringifyJson(values[i], level + 1);
        if (i + 1 < values.size()) out << ",";
        out << "\n";
    }
    out << indentString(level) << "]";
    return out.str();
}

static std::string stringifyObject(const std::map<std::string, JsonValue>& fields, int level) {
    if (fields.empty()) return "{}";
    std::ostringstream out;
    out << "{\n";
    size_t index = 0;
    for (auto& field : fields) {
        out << indentString(level + 1) << "\"" << escapeJsonString(field.first) << "\": "
            << stringifyJson(field.second, level + 1);
        if (index + 1 < fields.size()) out << ",";
        out << "\n";
        ++index;
    }
    out << indentString(level) << "}";
    return out.str();
}

static std::string stringifyJson(const JsonValue& value, int level) {
    switch (value.type) {
        case JsonValue::Type::Null:   return "null";
        case JsonValue::Type::Bool:   return value.boolValue ? "true" : "false";
        case JsonValue::Type::Number: {
            std::ostringstream out;
            out << std::setprecision(15) << value.numberValue;
            return out.str();
        }
        case JsonValue::Type::String:
            return std::string("\"") + escapeJsonString(value.stringValue) + "\"";
        case JsonValue::Type::Array:
            return stringifyArray(value.arrayValue, level);
        case JsonValue::Type::Object:
            return stringifyObject(value.objectValue, level);
    }
    return "null";
}

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text(text) {}

    bool parse(JsonValue& out) {
        skipWs();
        if (!parseValue(out)) return false;
        skipWs();
        return pos == text.size();
    }

private:
    const std::string& text;
    size_t pos = 0;

    void skipWs() {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
    }

    bool consume(char c) {
        if (pos < text.size() && text[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }

    bool parseValue(JsonValue& out) {
        skipWs();
        if (pos >= text.size()) return false;

        char c = text[pos];
        if (c == '"') return parseString(out);
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == 't') return parseLiteral("true", makeBool(true), out);
        if (c == 'f') return parseLiteral("false", makeBool(false), out);
        if (c == 'n') return parseLiteral("null", makeNull(), out);
        return parseNumber(out);
    }

    bool parseLiteral(const char* literal, const JsonValue& value, JsonValue& out) {
        size_t len = std::strlen(literal);
        if (text.compare(pos, len, literal) != 0) return false;
        pos += len;
        out = value;
        return true;
    }

    bool parseString(JsonValue& out) {
        if (!consume('"')) return false;
        std::string value;
        while (pos < text.size()) {
            char c = text[pos++];
            if (c == '"') {
                out = makeString(value);
                return true;
            }
            if (c != '\\') {
                value.push_back(c);
                continue;
            }

            if (pos >= text.size()) return false;
            char escape = text[pos++];
            switch (escape) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case 'u': {
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (pos >= text.size()) return false;
                        char h = text[pos++];
                        codepoint <<= 4;
                        if (h >= '0' && h <= '9') codepoint |= static_cast<unsigned int>(h - '0');
                        else if (h >= 'a' && h <= 'f') codepoint |= static_cast<unsigned int>(10 + h - 'a');
                        else if (h >= 'A' && h <= 'F') codepoint |= static_cast<unsigned int>(10 + h - 'A');
                        else return false;
                    }
                    appendUtf8(value, codepoint);
                    break;
                }
                default:
                    return false;
            }
        }
        return false;
    }

    bool parseNumber(JsonValue& out) {
        const char* start = text.c_str() + pos;
        char* end = nullptr;
        double value = std::strtod(start, &end);
        if (end == start) return false;
        pos = static_cast<size_t>(end - text.c_str());
        out = makeNumber(value);
        return true;
    }

    bool parseArray(JsonValue& out) {
        if (!consume('[')) return false;
        JsonValue array = makeArray();
        skipWs();
        if (consume(']')) {
            out = array;
            return true;
        }

        while (true) {
            JsonValue item;
            if (!parseValue(item)) return false;
            array.arrayValue.push_back(item);
            skipWs();
            if (consume(']')) {
                out = array;
                return true;
            }
            if (!consume(',')) return false;
            skipWs();
        }
    }

    bool parseObject(JsonValue& out) {
        if (!consume('{')) return false;
        JsonValue object = makeObject();
        skipWs();
        if (consume('}')) {
            out = object;
            return true;
        }

        while (true) {
            JsonValue keyValue;
            if (!parseString(keyValue)) return false;
            skipWs();
            if (!consume(':')) return false;
            JsonValue value;
            if (!parseValue(value)) return false;
            object.objectValue[keyValue.stringValue] = value;
            skipWs();
            if (consume('}')) {
                out = object;
                return true;
            }
            if (!consume(',')) return false;
            skipWs();
        }
    }
};

static bool parseJsonDocument(const std::string& path, JsonValue& out) {
    std::string content = readFile(path);
    if (content.empty()) return false;
    JsonParser parser(content);
    return parser.parse(out);
}

static bool saveJsonDocument(const std::string& path, const JsonValue& value) {
    return writeFile(path, stringifyJson(value) + "\n");
}

static const JsonValue* findField(const JsonValue& value, const std::string& key) {
    if (value.type != JsonValue::Type::Object) return nullptr;
    auto it = value.objectValue.find(key);
    if (it == value.objectValue.end()) return nullptr;
    return &it->second;
}

static const std::vector<JsonValue>* findArrayField(const JsonValue& value, const std::string& key) {
    const JsonValue* field = findField(value, key);
    if (!field || field->type != JsonValue::Type::Array) return nullptr;
    return &field->arrayValue;
}

static std::string fieldString(const JsonValue& value, const std::string& key,
                               const std::string& fallback = "") {
    const JsonValue* field = findField(value, key);
    if (!field || field->type != JsonValue::Type::String) return fallback;
    return field->stringValue;
}

static double fieldNumber(const JsonValue& value, const std::string& key, double fallback = 0.0) {
    const JsonValue* field = findField(value, key);
    if (!field) return fallback;
    if (field->type == JsonValue::Type::Number) return field->numberValue;
    if (field->type == JsonValue::Type::String) {
        try { return std::stod(field->stringValue); } catch (...) { return fallback; }
    }
    return fallback;
}

static bool fieldBool(const JsonValue& value, const std::string& key, bool fallback = false) {
    const JsonValue* field = findField(value, key);
    if (!field) return fallback;
    if (field->type == JsonValue::Type::Bool) return field->boolValue;
    if (field->type == JsonValue::Type::String)
        return field->stringValue == "1" || field->stringValue == "true";
    return fallback;
}

static JsonValue userToJson(const User& user) {
    return makeObject({
        {"id", makeString(user.id)},
        {"username", makeString(user.username)},
        {"name", makeString(user.name)},
        {"email", makeString(user.email)},
        {"password", makeString(user.password)},
        {"created", makeString(user.created)}
    });
}

static User userFromJson(const JsonValue& value) {
    User user;
    user.id = fieldString(value, "id");
    user.username = fieldString(value, "username");
    user.name = fieldString(value, "name");
    user.email = fieldString(value, "email");
    user.password = fieldString(value, "password");
    user.created = fieldString(value, "created");
    return user;
}

static JsonValue transactionToJson(const Transaction& t) {
    return makeObject({
        {"id", makeString(t.id)},
        {"type", makeString(t.type)},
        {"amount", makeNumber(t.amount)},
        {"category", makeString(t.category)},
        {"date", makeString(t.date)},
        {"description", makeString(t.description)},
        {"eventId", makeString(t.eventId)},
        {"expenseKind", makeString(t.expenseKind)}
    });
}

static Transaction transactionFromJson(const JsonValue& value) {
    Transaction t;
    t.id = fieldString(value, "id");
    t.type = fieldString(value, "type");
    t.amount = fieldNumber(value, "amount");
    t.category = fieldString(value, "category");
    t.date = fieldString(value, "date");
    t.description = fieldString(value, "description");
    t.eventId = fieldString(value, "eventId");
    t.expenseKind = fieldString(value, "expenseKind");
    if (t.type == "expense" && t.expenseKind.empty())
        t.expenseKind = !t.eventId.empty() ? "event" : "daily";
    return t;
}

static JsonValue categoryToJson(const Category& c) {
    return makeObject({
        {"id", makeString(c.id)},
        {"name", makeString(c.name)},
        {"type", makeString(c.type)}
    });
}

static Category categoryFromJson(const JsonValue& value) {
    Category c;
    c.id = fieldString(value, "id");
    c.name = fieldString(value, "name");
    c.type = fieldString(value, "type");
    return c;
}

static JsonValue budgetToJson(const BudgetSettings& b) {
    return makeObject({
        {"month", makeString(b.month)},
        {"expectedIncome", makeNumber(b.expectedIncome)},
        {"savings", makeNumber(b.savings)},
        {"fixedExpense", makeNumber(b.fixedExpense)},
        {"configured", makeBool(b.configured)}
    });
}

static BudgetSettings budgetFromJson(const JsonValue& value) {
    BudgetSettings b;
    b.month = fieldString(value, "month");
    b.expectedIncome = fieldNumber(value, "expectedIncome");
    b.savings = fieldNumber(value, "savings");
    b.fixedExpense = fieldNumber(value, "fixedExpense");
    b.configured = fieldBool(value, "configured");
    return b;
}

static JsonValue eventToJson(const SpecialEvent& e) {
    return makeObject({
        {"id", makeString(e.id)},
        {"name", makeString(e.name)},
        {"startDate", makeString(e.startDate)},
        {"endDate", makeString(e.endDate)},
        {"description", makeString(e.description)},
        {"budget", makeNumber(e.budget)},
        {"spent", makeNumber(e.spent)}
    });
}

static SpecialEvent eventFromJson(const JsonValue& value) {
    SpecialEvent e;
    e.id = fieldString(value, "id");
    e.name = fieldString(value, "name");
    e.startDate = fieldString(value, "startDate");
    e.endDate = fieldString(value, "endDate");
    e.description = fieldString(value, "description");
    e.budget = fieldNumber(value, "budget");
    e.spent = fieldNumber(value, "spent");
    return e;
}

static JsonValue splitEntryToJson(const SplitEntry& s) {
    return makeObject({
        {"memberId", makeString(s.memberId)},
        {"amount", makeNumber(s.amount)}
    });
}

static SplitEntry splitEntryFromJson(const JsonValue& value) {
    SplitEntry s;
    s.memberId = fieldString(value, "memberId");
    s.amount = fieldNumber(value, "amount");
    return s;
}

static JsonValue groupMemberToJson(const GroupMember& m) {
    return makeObject({
        {"id", makeString(m.id)},
        {"name", makeString(m.name)}
    });
}

static GroupMember groupMemberFromJson(const JsonValue& value) {
    GroupMember m;
    m.id = fieldString(value, "id");
    m.name = fieldString(value, "name");
    return m;
}

static JsonValue groupExpenseToJson(const GroupExpense& ex) {
    std::vector<JsonValue> splits;
    for (auto& s : ex.splits) splits.push_back(splitEntryToJson(s));

    return makeObject({
        {"id", makeString(ex.id)},
        {"description", makeString(ex.description)},
        {"date", makeString(ex.date)},
        {"paidBy", makeString(ex.paidBy)},
        {"amount", makeNumber(ex.amount)},
        {"splits", makeArray(splits)}
    });
}

static GroupExpense groupExpenseFromJson(const JsonValue& value) {
    GroupExpense ex;
    ex.id = fieldString(value, "id");
    ex.description = fieldString(value, "description");
    ex.date = fieldString(value, "date");
    ex.paidBy = fieldString(value, "paidBy");
    ex.amount = fieldNumber(value, "amount");

    const std::vector<JsonValue>* splits = findArrayField(value, "splits");
    if (splits) {
        for (auto& split : *splits)
            ex.splits.push_back(splitEntryFromJson(split));
    }
    return ex;
}

static JsonValue groupToJson(const Group& g) {
    std::vector<JsonValue> members;
    for (auto& m : g.members) members.push_back(groupMemberToJson(m));

    std::vector<JsonValue> expenses;
    for (auto& ex : g.expenses) expenses.push_back(groupExpenseToJson(ex));

    return makeObject({
        {"id", makeString(g.id)},
        {"name", makeString(g.name)},
        {"created", makeString(g.created)},
        {"members", makeArray(members)},
        {"expenses", makeArray(expenses)}
    });
}

static Group groupFromJson(const JsonValue& value) {
    Group g;
    g.id = fieldString(value, "id");
    g.name = fieldString(value, "name");
    g.created = fieldString(value, "created");

    const std::vector<JsonValue>* members = findArrayField(value, "members");
    if (members) {
        for (auto& member : *members)
            g.members.push_back(groupMemberFromJson(member));
    }

    const std::vector<JsonValue>* expenses = findArrayField(value, "expenses");
    if (expenses) {
        for (auto& expense : *expenses)
            g.expenses.push_back(groupExpenseFromJson(expense));
    }
    return g;
}

static std::vector<User> loadUsersLegacy() {
    std::vector<User> users;
    for (auto& line : readLines(dataPath("users.txt"))) {
        auto f = Utils::splitStr(line, '|');
        if (f.size() < 6) continue;
        users.push_back({f[0], f[1], f[2], f[3], f[4], f[5]});
    }
    return users;
}

static std::vector<Transaction> loadTransactionsLegacy(const std::string& uid) {
    std::vector<Transaction> txs;
    for (auto& line : readLines(dataPath("tx_" + uid + ".txt"))) {
        auto f = Utils::splitStr(line, '|');
        if (f.size() < 7) continue;
        Transaction t;
        t.id          = f[0];
        t.type        = f[1];
        try { t.amount = std::stod(f[2]); } catch (...) { t.amount = 0; }
        t.category    = f[3];
        t.date        = f[4];
        t.description = f[5];
        t.eventId     = f[6];
        t.expenseKind = (f.size() >= 8) ? f[7] : "";
        if (t.type == "expense" && t.expenseKind.empty())
            t.expenseKind = !t.eventId.empty() ? "event" : "daily";
        txs.push_back(t);
    }
    return txs;
}

static std::vector<Category> loadCategoriesLegacy(const std::string& uid) {
    auto lines = readLines(dataPath("cats_" + uid + ".txt"));
    if (lines.empty()) return {};
    std::vector<Category> cats;
    for (auto& line : lines) {
        auto f = Utils::splitStr(line, '|');
        if (f.size() < 3) continue;
        cats.push_back({f[0], f[1], f[2]});
    }
    return cats;
}

static std::vector<BudgetSettings> loadBudgetLegacyEntries(const std::string& uid) {
    std::vector<BudgetSettings> budgets;
    for (auto& line : readLines(dataPath("budget_" + uid + ".txt"))) {
        auto f = Utils::splitStr(line, '|');
        if (f.size() < 5) continue;
        BudgetSettings b;
        b.month = f[0];
        try {
            b.expectedIncome = std::stod(f[1]);
            b.savings        = std::stod(f[2]);
            b.fixedExpense   = std::stod(f[3]);
            b.configured     = (f[4] == "1" || f[4] == "true");
        } catch (...) {}
        budgets.push_back(b);
    }
    return budgets;
}

static std::vector<SpecialEvent> loadEventsLegacy(const std::string& uid) {
    std::vector<SpecialEvent> evs;
    for (auto& line : readLines(dataPath("events_" + uid + ".txt"))) {
        auto f = Utils::splitStr(line, '|');
        if (f.size() < 7) continue;
        SpecialEvent e;
        e.id          = f[0];
        e.name        = f[1];
        e.startDate   = f[2];
        e.endDate     = f[3];
        try { e.budget = std::stod(f[4]); } catch (...) { e.budget = 0; }
        try { e.spent  = std::stod(f[5]); } catch (...) { e.spent  = 0; }
        e.description = f[6];
        evs.push_back(e);
    }
    return evs;
}

static std::vector<Group> loadGroupsLegacy(const std::string& uid) {
    std::vector<Group> groups;
    std::ifstream file(dataPath("groups_" + uid + ".txt"));
    if (!file.is_open()) return groups;

    std::string line;
    Group* cur = nullptr;
    GroupExpense* curExp = nullptr;

    while (std::getline(file, line)) {
        line = Utils::trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto f = Utils::splitStr(line, '|');
        if (f.empty()) continue;

        if (f[0] == "GROUP" && f.size() >= 4) {
            groups.push_back({});
            cur = &groups.back();
            curExp = nullptr;
            cur->id      = f[1];
            cur->name    = f[2];
            cur->created = f[3];
        } else if (f[0] == "MEMBER" && f.size() >= 3 && cur) {
            cur->members.push_back({f[1], f[2]});
        } else if (f[0] == "EXPENSE" && f.size() >= 6 && cur) {
            GroupExpense ex;
            ex.id          = f[1];
            ex.description = f[2];
            try { ex.amount = std::stod(f[3]); } catch (...) { ex.amount = 0; }
            ex.date        = f[4];
            ex.paidBy      = f[5];
            cur->expenses.push_back(ex);
            curExp = &cur->expenses.back();
        } else if (f[0] == "SPLIT" && f.size() >= 3 && curExp) {
            double amt = 0.0;
            try { amt = std::stod(f[2]); } catch (...) {}
            curExp->splits.push_back({f[1], amt});
        } else if (f[0] == "END") {
            cur    = nullptr;
            curExp = nullptr;
        }
    }
    return groups;
}

static std::vector<JsonValue> buildUsersJsonArray(const std::vector<User>& users) {
    std::vector<JsonValue> items;
    for (auto& user : users) items.push_back(userToJson(user));
    return items;
}

static std::vector<JsonValue> buildTransactionsJsonArray(const std::vector<Transaction>& txs) {
    std::vector<JsonValue> items;
    for (auto& tx : txs) items.push_back(transactionToJson(tx));
    return items;
}

static std::vector<JsonValue> buildCategoriesJsonArray(const std::vector<Category>& cats) {
    std::vector<JsonValue> items;
    for (auto& cat : cats) items.push_back(categoryToJson(cat));
    return items;
}

static std::vector<JsonValue> buildBudgetsJsonArray(const std::vector<BudgetSettings>& budgets) {
    std::vector<JsonValue> items;
    for (auto& budget : budgets) items.push_back(budgetToJson(budget));
    return items;
}

static std::vector<JsonValue> buildEventsJsonArray(const std::vector<SpecialEvent>& events) {
    std::vector<JsonValue> items;
    for (auto& event : events) items.push_back(eventToJson(event));
    return items;
}

static std::vector<JsonValue> buildGroupsJsonArray(const std::vector<Group>& groups) {
    std::vector<JsonValue> items;
    for (auto& group : groups) items.push_back(groupToJson(group));
    return items;
}

static JsonValue loadJsonRootOrNull(const std::string& path) {
    JsonValue root;
    if (!parseJsonDocument(path, root)) return makeNull();
    return root;
}

// --- Init -----------------------------------------------

void Storage::init() {
    ensureDir();
}

// --- Users ----------------------------------------------
// JSON: { "users": [ ... ] }

std::vector<User> Storage::loadUsers() {
    std::vector<User> users;

    if (fileExists(dataPath("users.json"))) {
        JsonValue root = loadJsonRootOrNull(dataPath("users.json"));
        const std::vector<JsonValue>* items = findArrayField(root, "users");
        if (!items && root.type == JsonValue::Type::Array)
            items = &root.arrayValue;
        if (items) {
            for (auto& item : *items)
                users.push_back(userFromJson(item));
            return users;
        }
    }

    return loadUsersLegacy();
}

void Storage::saveUsers(const std::vector<User>& users) {
    JsonValue root = makeObject({{"users", makeArray(buildUsersJsonArray(users))}});
    saveJsonDocument(dataPath("users.json"), root);
}

// --- Transactions ---------------------------------------
// JSON: { "transactions": [ ... ] }

std::vector<Transaction> Storage::loadTransactions(const std::string& uid) {
    std::vector<Transaction> txs;

    std::string jsonFile = dataPath("tx_" + uid + ".json");
    if (fileExists(jsonFile)) {
        JsonValue root = loadJsonRootOrNull(jsonFile);
        const std::vector<JsonValue>* items = findArrayField(root, "transactions");
        if (!items && root.type == JsonValue::Type::Array)
            items = &root.arrayValue;
        if (items) {
            for (auto& item : *items)
                txs.push_back(transactionFromJson(item));
            return txs;
        }
    }

    return loadTransactionsLegacy(uid);
}

void Storage::saveTransactions(const std::string& uid,
                                const std::vector<Transaction>& txs) {
    JsonValue root = makeObject({{"transactions", makeArray(buildTransactionsJsonArray(txs))}});
    saveJsonDocument(dataPath("tx_" + uid + ".json"), root);
}

// --- Categories -----------------------------------------
// JSON: { "categories": [ ... ] }

static std::vector<Category> defaultCategories() {
    return {
        {"c01", "Food",          "expense"},
        {"c02", "Transport",     "expense"},
        {"c03", "Housing",       "expense"},
        {"c04", "Entertainment", "expense"},
        {"c05", "Medical",       "expense"},
        {"c06", "Shopping",      "expense"},
        {"c07", "Education",     "expense"},
        {"c08", "Salary",        "income"},
        {"c09", "Freelance",     "income"},
        {"c10", "Investment",    "income"},
        {"c11", "Other",         "both"},
    };
}

std::vector<Category> Storage::loadCategories(const std::string& uid) {
    std::vector<Category> cats;

    std::string jsonFile = dataPath("cats_" + uid + ".json");
    if (fileExists(jsonFile)) {
        JsonValue root = loadJsonRootOrNull(jsonFile);
        const std::vector<JsonValue>* items = findArrayField(root, "categories");
        if (!items && root.type == JsonValue::Type::Array)
            items = &root.arrayValue;
        if (items) {
            for (auto& item : *items)
                cats.push_back(categoryFromJson(item));
            if (!cats.empty()) return cats;
        }
    }

    cats = loadCategoriesLegacy(uid);
    if (cats.empty()) return defaultCategories();
    return cats;
}

void Storage::saveCategories(const std::string& uid,
                              const std::vector<Category>& cats) {
    JsonValue root = makeObject({{"categories", makeArray(buildCategoriesJsonArray(cats))}});
    saveJsonDocument(dataPath("cats_" + uid + ".json"), root);
}

// --- Budget Settings (per-month) ------------------------
// JSON: { "budgets": [ ... ] }

BudgetSettings Storage::loadBudget(const std::string& uid,
                                    const std::string& ym) {
    BudgetSettings result;
    result.month = ym;
    result.configured = false;

    std::string jsonFile = dataPath("budget_" + uid + ".json");
    if (fileExists(jsonFile)) {
        JsonValue root = loadJsonRootOrNull(jsonFile);
        const std::vector<JsonValue>* items = findArrayField(root, "budgets");
        if (!items && root.type == JsonValue::Type::Array)
            items = &root.arrayValue;
        if (items) {
            for (auto& item : *items) {
                BudgetSettings b = budgetFromJson(item);
                if (b.month == ym) return b;
            }
        }
    }

    for (auto& b : loadBudgetLegacyEntries(uid)) {
        if (b.month == ym) return b;
    }

    return result;
}

void Storage::saveBudget(const std::string& uid,
                          const BudgetSettings& b) {
    std::vector<BudgetSettings> budgets;

    std::string jsonFile = dataPath("budget_" + uid + ".json");
    if (fileExists(jsonFile)) {
        JsonValue root = loadJsonRootOrNull(jsonFile);
        const std::vector<JsonValue>* items = findArrayField(root, "budgets");
        if (!items && root.type == JsonValue::Type::Array)
            items = &root.arrayValue;
        if (items) {
            for (auto& item : *items)
                budgets.push_back(budgetFromJson(item));
        }
    } else {
        budgets = loadBudgetLegacyEntries(uid);
    }

    std::vector<BudgetSettings> kept;
    for (auto& existing : budgets) {
        if (existing.month != b.month)
            kept.push_back(existing);
    }
    kept.push_back(b);

    JsonValue root = makeObject({{"budgets", makeArray(buildBudgetsJsonArray(kept))}});
    saveJsonDocument(jsonFile, root);
}

// --- Special Events -------------------------------------
// JSON: { "events": [ ... ] }

std::vector<SpecialEvent> Storage::loadEvents(const std::string& uid) {
    std::vector<SpecialEvent> evs;

    std::string jsonFile = dataPath("events_" + uid + ".json");
    if (fileExists(jsonFile)) {
        JsonValue root = loadJsonRootOrNull(jsonFile);
        const std::vector<JsonValue>* items = findArrayField(root, "events");
        if (!items && root.type == JsonValue::Type::Array)
            items = &root.arrayValue;
        if (items) {
            for (auto& item : *items)
                evs.push_back(eventFromJson(item));
            return evs;
        }
    }

    return loadEventsLegacy(uid);
}

void Storage::saveEvents(const std::string& uid,
                          const std::vector<SpecialEvent>& evs) {
    JsonValue root = makeObject({{"events", makeArray(buildEventsJsonArray(evs))}});
    saveJsonDocument(dataPath("events_" + uid + ".json"), root);
}

// --- Groups (per-user, multi-line block format) ---------
//
// JSON: { "groups": [ ... ] }
//
// The legacy block format is still readable as a migration path.
//

std::vector<Group> Storage::loadGroups(const std::string& uid) {
    std::vector<Group> groups;

    std::string jsonFile = dataPath("groups_" + uid + ".json");
    if (fileExists(jsonFile)) {
        JsonValue root = loadJsonRootOrNull(jsonFile);
        const std::vector<JsonValue>* items = findArrayField(root, "groups");
        if (!items && root.type == JsonValue::Type::Array)
            items = &root.arrayValue;
        if (items) {
            for (auto& item : *items)
                groups.push_back(groupFromJson(item));
            return groups;
        }
    }

    return loadGroupsLegacy(uid);
}

void Storage::saveGroups(const std::string& uid,
                          const std::vector<Group>& groups) {
    JsonValue root = makeObject({{"groups", makeArray(buildGroupsJsonArray(groups))}});
    saveJsonDocument(dataPath("groups_" + uid + ".json"), root);
}
