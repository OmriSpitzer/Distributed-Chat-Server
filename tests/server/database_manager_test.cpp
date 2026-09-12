/**
 * DatabaseManager unit tests
 *
 * @brief Includes: singleton, seed rows, getUser / userExists edges, createUser constraints,
 * login success and failures, saveMessage ignore and FK, loadHistory order and limit,
 * presence and membership, concurrent writes, typical register/login/logout flow.
 * @date 12-09-2026
 */

#include "config/config.h"
#include "server/database_manager.h"
#include "utils/models/message.h"
#include "utils/models/user.h"
#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <variant>
#include <vector>

/**
 * 1. singleton identity
 * 2. seed users
 * 3. getUser edges
 * 4. userExists edges
 * 5. createUser success
 * 6. createUser constraints
 * 7. createUser string edges
 * 8. loginUser success
 * 9. loginUser failures
 * 10. saveMessage success and duplicate id
 * 11. saveMessage foreign keys
 * 12. saveMessage content edges
 * 13. loadHistory order and reconstruct
 * 14. loadHistory limit
 * 15. online presence
 * 16. membership no-throw edges
 * 17. concurrent writes
 * 18. typical register / login / logout flow
 */

namespace {
constexpr const char *kAuthFailed = "Invalid username or password";
constexpr const char *kLobby = "Lobby";
constexpr const char *kSeedHash = "$argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>";

std::string unique(std::string_view prefix) {
  static std::atomic<std::uint64_t> seq{0};
  const auto n = seq.fetch_add(1);
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::string(prefix) + "_" + std::to_string(n) + "_" + std::to_string(now);
}

DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path() / "dcs-db-manager-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / ("node-" + unique("pid") + "-" + std::to_string(rd()) + ".db");
    config::DB_PATH = path.string();
    return &DatabaseManager::getInstance();
  }();
  return *instance;
}

User createUniqueUser(std::string_view password = "secret") {
  const std::string name = unique("user");
  return db().createUser(name, password, name + "@example.com");
}

User guest() { return User("", "", User::UserType::GUEST); }

Message makeStored(const User &from, std::string_view content, std::string id, std::time_t ts) {
  return Message(from, guest(), content, std::move(id), ts);
}
} // namespace

// 1. singleton identity
TEST_CASE("DatabaseManager singleton identity", "[database_manager][singleton]") {
  DatabaseManager &a = db();
  DatabaseManager &b = DatabaseManager::getInstance();
  REQUIRE(&a == &b);
}

// 2. seed users
TEST_CASE("DatabaseManager seed users", "[database_manager][seed]") {
  DatabaseManager &database = db();

  SECTION("omri exists as USER") {
    REQUIRE(database.userExists("omri"));
    const auto user = database.getUser("omri");
    REQUIRE(user.has_value());
    REQUIRE(user->getUsername() == "omri");
    REQUIRE(user->getEmail() == "omri@gmail.com");
    REQUIRE(user->getUserType() == User::UserType::USER);
  }

  SECTION("spitzer exists as USER") {
    REQUIRE(database.userExists("spitzer"));
    const auto user = database.getUser("spitzer");
    REQUIRE(user.has_value());
    REQUIRE(user->getEmail() == "spitzer@gmail.com");
    REQUIRE(user->getUserType() == User::UserType::USER);
  }

  SECTION("admin exists as ADMIN") {
    REQUIRE(database.userExists("admin"));
    const auto user = database.getUser("admin");
    REQUIRE(user.has_value());
    REQUIRE(user->getEmail() == "admin@gmail.com");
    REQUIRE(user->getUserType() == User::UserType::ADMIN);
  }

  SECTION("seed users are not online") {
    REQUIRE_FALSE(database.isUserOnline("omri"));
    REQUIRE_FALSE(database.isUserOnline("spitzer"));
    REQUIRE_FALSE(database.isUserOnline("admin"));
  }
}

// 3. getUser edges
TEST_CASE("DatabaseManager getUser edges", "[database_manager][getUser][edge]") {
  DatabaseManager &database = db();

  SECTION("missing username") {
    REQUIRE_FALSE(database.getUser(unique("missing")).has_value());
  }

  SECTION("empty username") {
    REQUIRE_FALSE(database.getUser("").has_value());
  }

  SECTION("case does not match seed") {
    REQUIRE_FALSE(database.getUser("OMRI").has_value());
    REQUIRE_FALSE(database.getUser("Omri").has_value());
    REQUIRE_FALSE(database.getUser("Admin").has_value());
  }

  SECTION("surrounding whitespace is not trimmed") {
    REQUIRE_FALSE(database.getUser(" omri ").has_value());
    REQUIRE_FALSE(database.getUser("omri ").has_value());
  }

  SECTION("sql metacharacters are bound, not injected") {
    REQUIRE_FALSE(database.getUser("' OR 1=1 --").has_value());
    REQUIRE_FALSE(database.getUser("omri'; DROP TABLE users;--").has_value());
    REQUIRE(database.userExists("omri"));
  }
}

// 4. userExists edges
TEST_CASE("DatabaseManager userExists edges", "[database_manager][userExists][edge]") {
  DatabaseManager &database = db();

  SECTION("false for unknown / empty") {
    REQUIRE_FALSE(database.userExists(unique("ghost")));
    REQUIRE_FALSE(database.userExists(""));
    std::string withNul("omri");
    withNul.push_back('\0');
    withNul += "extra";
    REQUIRE_FALSE(database.userExists(withNul));
  }

  SECTION("true after createUser") {
    const User user = createUniqueUser();
    REQUIRE(database.userExists(user.getUsername()));
    REQUIRE_FALSE(database.userExists(user.getUsername() + "_x"));
  }
}

// 5. createUser success
TEST_CASE("DatabaseManager createUser success", "[database_manager][createUser]") {
  DatabaseManager &database = db();
  const std::string name = unique("new");
  const User created = database.createUser(name, "pw", name + "@mail.test");

  REQUIRE(created.getUsername() == name);
  REQUIRE(created.getEmail() == name + "@mail.test");
  REQUIRE(created.getUserType() == User::UserType::USER);

  const auto loaded = database.getUser(name);
  REQUIRE(loaded.has_value());
  REQUIRE(*loaded == created);
  REQUIRE(loaded->getUserType() == User::UserType::USER);
}

// 6. createUser constraints
TEST_CASE("DatabaseManager createUser constraints", "[database_manager][createUser][edge]") {
  DatabaseManager &database = db();
  const std::string name = unique("dup");
  const std::string email = name + "@mail.test";
  database.createUser(name, "pw", email);

  SECTION("duplicate username") {
    REQUIRE_THROWS_AS(database.createUser(name, "other", unique("e") + "@mail.test"),
                      DatabaseManager::ConstraintError);
    try {
      database.createUser(name, "other", unique("e") + "@mail.test");
      FAIL("expected ConstraintError");
    } catch (const DatabaseManager::ConstraintError &ex) {
      REQUIRE(std::string(ex.what()) == "User already exists");
    }
  }

  SECTION("duplicate email is also reported as already exists") {
    REQUIRE_THROWS_AS(database.createUser(unique("other"), "pw", email),
                      DatabaseManager::ConstraintError);
    try {
      database.createUser(unique("other"), "pw", email);
      FAIL("expected ConstraintError");
    } catch (const std::runtime_error &ex) {
      REQUIRE(std::string(ex.what()) == "User already exists");
    }
  }

  SECTION("seed username is taken") {
    REQUIRE_THROWS_AS(database.createUser("omri", "pw", unique("omri") + "@mail.test"),
                      DatabaseManager::ConstraintError);
  }

  SECTION("seed email is taken") {
    REQUIRE_THROWS_AS(database.createUser(unique("taken"), "pw", "admin@gmail.com"),
                      DatabaseManager::ConstraintError);
  }
}

// 7. createUser string edges
TEST_CASE("DatabaseManager createUser string edges", "[database_manager][createUser][edge]") {
  DatabaseManager &database = db();

  SECTION("empty username is stored") {
    const std::string email = unique("empty") + "@mail.test";
    if (!database.userExists("")) {
      const User created = database.createUser("", "pw", email);
      REQUIRE(created.getUsername().empty());
      REQUIRE(database.userExists(""));
      const auto loaded = database.getUser("");
      REQUIRE(loaded.has_value());
      REQUIRE(loaded->getEmail() == email);
    }
  }

  SECTION("empty email is stored") {
    const std::string name = unique("noemail");
    const User created = database.createUser(name, "pw", "");
    REQUIRE(created.getEmail().empty());
    REQUIRE(database.getUser(name)->getEmail().empty());
  }

  SECTION("empty password still creates a row") {
    const User created = createUniqueUser("");
    REQUIRE(database.userExists(created.getUsername()));
  }

  SECTION("unicode username and email") {
    const std::string name = unique("עֹמְרִי");
    const User created = database.createUser(name, "סוד", name + "@דוגמה.com");
    const auto loaded = database.getUser(name);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->getUsername() == name);
    REQUIRE(loaded->getEmail() == name + "@דוגמה.com");
  }

  SECTION("whitespace is significant") {
    const std::string name = unique(" spaced ");
    database.createUser(name, "pw", name + "@mail.test");
    REQUIRE(database.userExists(name));
    REQUIRE_FALSE(database.userExists(name.substr(1)));
  }

  SECTION("quotes and separators") {
    const std::string name = unique("o'reilly,;\"\\");
    const User created = database.createUser(name, "p'w", name + "@mail.test");
    REQUIRE(database.getUser(name)->getUsername() == created.getUsername());
  }

  SECTION("long username and email") {
    const std::string name = unique("l") + std::string(4000, 'x');
    const std::string email = name + "@mail.test";
    const User created = database.createUser(name, "pw", email);
    REQUIRE(database.getUser(name)->getEmail() == email);
    REQUIRE(created.getUsername().size() > 4000);
  }
}

// 8. loginUser success
TEST_CASE("DatabaseManager loginUser success", "[database_manager][login]") {
  DatabaseManager &database = db();
  const std::string name = unique("login");
  const std::string password = "correct horse";
  const User created = database.createUser(name, password, name + "@mail.test");

  const auto result = database.loginUser(name, password);
  REQUIRE(std::holds_alternative<User>(result));
  const User &logged = std::get<User>(result);
  REQUIRE(logged.getUsername() == name);
  REQUIRE(logged.getEmail() == created.getEmail());
  REQUIRE(logged.getUserType() == User::UserType::USER);
}

// 9. loginUser failures
TEST_CASE("DatabaseManager loginUser failures", "[database_manager][login][edge]") {
  DatabaseManager &database = db();
  const std::string name = unique("badlogin");
  database.createUser(name, "right", name + "@mail.test");

  auto expectAuthFailed = [](const std::variant<User, std::string> &result) {
    REQUIRE(std::holds_alternative<std::string>(result));
    REQUIRE(std::get<std::string>(result) == kAuthFailed);
  };

  SECTION("unknown user") {
    expectAuthFailed(database.loginUser(unique("nobody"), "pw"));
  }

  SECTION("wrong password") {
    expectAuthFailed(database.loginUser(name, "wrong"));
  }

  SECTION("empty password against a real hash") {
    expectAuthFailed(database.loginUser(name, ""));
  }

  SECTION("empty username") {
    expectAuthFailed(database.loginUser("", "pw"));
  }

  SECTION("case mismatch") {
    expectAuthFailed(database.loginUser("OMRI", "pw"));
  }

  SECTION("seed placeholder hash rejects a normal password") {
    expectAuthFailed(database.loginUser("omri", "password"));
    expectAuthFailed(database.loginUser("admin", "admin"));
  }

  SECTION("seed placeholder hash accepts the stored string as plaintext fallback") {
    const auto result = database.loginUser("omri", kSeedHash);
    REQUIRE(std::holds_alternative<User>(result));
    REQUIRE(std::get<User>(result).getUsername() == "omri");
  }
}

// 10. saveMessage success and duplicate id
TEST_CASE("DatabaseManager saveMessage success and duplicate id",
          "[database_manager][saveMessage]") {
  DatabaseManager &database = db();
  const User from = createUniqueUser();
  const Message first = makeStored(from, "hello", unique("msg"), 1'700'000'000);

  REQUIRE(database.saveMessage(first, kLobby));
  REQUIRE_FALSE(database.saveMessage(first, kLobby));

  const Message sameId = makeStored(from, "changed", first.getId(), 1'700'000'001);
  REQUIRE_FALSE(database.saveMessage(sameId, kLobby));
}

// 11. saveMessage foreign keys
TEST_CASE("DatabaseManager saveMessage foreign keys", "[database_manager][saveMessage][edge]") {
  DatabaseManager &database = db();
  const User from = createUniqueUser();
  const Message ok = makeStored(from, "fk", unique("fk"), 1'700'000'010);

  SECTION("unknown room") {
    REQUIRE_THROWS_AS(database.saveMessage(ok, unique("room")), DatabaseManager::ConstraintError);
  }

  SECTION("empty room is not Lobby") {
    REQUIRE_THROWS_AS(database.saveMessage(ok, ""), DatabaseManager::ConstraintError);
  }

  SECTION("unknown sender") {
    const User ghost(unique("ghost"), "g@mail.test", User::UserType::USER);
    const Message msg = makeStored(ghost, "hi", unique("ghostmsg"), 1'700'000'011);
    REQUIRE_THROWS_AS(database.saveMessage(msg, kLobby), DatabaseManager::ConstraintError);
  }

  SECTION("anonymous sender is not a users row") {
    const Message msg = makeStored(User::anonymousUser(), "hi", unique("anonmsg"), 1'700'000'012);
    REQUIRE_THROWS_AS(database.saveMessage(msg, kLobby), DatabaseManager::ConstraintError);
  }
}

// 12. saveMessage content edges
TEST_CASE("DatabaseManager saveMessage content edges", "[database_manager][saveMessage][edge]") {
  DatabaseManager &database = db();
  const User from = createUniqueUser();
  const std::time_t base = 8'000'000'000;

  auto saved = [&](std::string_view content, std::time_t ts) {
    const Message msg = makeStored(from, content, unique("c"), ts);
    REQUIRE(database.saveMessage(msg, kLobby));
    return msg;
  };

  SECTION("empty content") {
    const Message msg = saved("", base);
    const auto history = database.loadHistory(kLobby);
    const auto it = std::find_if(history.begin(), history.end(),
                                 [&](const Message &row) { return row.getId() == msg.getId(); });
    REQUIRE(it != history.end());
    REQUIRE(it->getContent().empty());
  }

  SECTION("unicode and quotes") {
    const std::string content = "שלום '\"\\; --";
    const Message msg = saved(content, base + 1);
    const auto history = database.loadHistory(kLobby);
    const auto it = std::find_if(history.begin(), history.end(),
                                 [&](const Message &row) { return row.getId() == msg.getId(); });
    REQUIRE(it != history.end());
    REQUIRE(it->getContent() == content);
  }

  SECTION("long content") {
    const std::string content(20'000, 'm');
    const Message msg = saved(content, base + 2);
    const auto history = database.loadHistory(kLobby);
    const auto it = std::find_if(history.begin(), history.end(),
                                 [&](const Message &row) { return row.getId() == msg.getId(); });
    REQUIRE(it != history.end());
    REQUIRE(it->getContent().size() == content.size());
  }

  SECTION("embedded nul is truncated on load") {
    std::string content = "hello";
    content.push_back('\0');
    content += "world";
    const Message msg = saved(content, base + 3);
    const auto history = database.loadHistory(kLobby);
    const auto it = std::find_if(history.begin(), history.end(),
                                 [&](const Message &row) { return row.getId() == msg.getId(); });
    REQUIRE(it != history.end());
    REQUIRE(it->getContent() == "hello");
  }
}

// 13. loadHistory order and reconstruct
TEST_CASE("DatabaseManager loadHistory order and reconstruct",
          "[database_manager][loadHistory]") {
  DatabaseManager &database = db();
  const User from = createUniqueUser();
  const std::time_t base = 9'000'000'000;
  const Message older = makeStored(from, "older", unique("h"), base);
  const Message middle = makeStored(from, "middle", unique("h"), base + 5);
  const Message newest = makeStored(from, "newest", unique("h"), base + 10);

  REQUIRE(database.saveMessage(older, kLobby));
  REQUIRE(database.saveMessage(newest, kLobby));
  REQUIRE(database.saveMessage(middle, kLobby));

  SECTION("unknown room is empty") {
    REQUIRE(database.loadHistory(unique("noroom")).empty());
    REQUIRE(database.loadHistory("").empty());
  }

  SECTION("newest first among inserted ids") {
    const auto history = database.loadHistory(kLobby);
    std::vector<std::string> ids;
    for (const Message &row : history) {
      if (row.getId() == newest.getId() || row.getId() == middle.getId() ||
          row.getId() == older.getId()) {
        ids.push_back(row.getId());
      }
    }
    REQUIRE(ids.size() == 3);
    REQUIRE(ids[0] == newest.getId());
    REQUIRE(ids[1] == middle.getId());
    REQUIRE(ids[2] == older.getId());
  }

  SECTION("reconstructed sender fields") {
    const auto history = database.loadHistory(kLobby);
    const auto it = std::find_if(history.begin(), history.end(), [&](const Message &row) {
      return row.getId() == newest.getId();
    });
    REQUIRE(it != history.end());
    REQUIRE(it->getContent() == "newest");
    REQUIRE(it->getTimestamp() == newest.getTimestamp());
    REQUIRE(it->getFrom().getUsername() == from.getUsername());
    REQUIRE(it->getFrom().getEmail() == from.getEmail());
    REQUIRE(it->getFrom().getUserType() == User::UserType::USER);
    REQUIRE(it->getTo().getUsername().empty());
    REQUIRE(it->getTo().getUserType() == User::UserType::GUEST);
  }
}

// 14. loadHistory limit
TEST_CASE("DatabaseManager loadHistory limit", "[database_manager][loadHistory][edge]") {
  DatabaseManager &database = db();
  const User from = createUniqueUser();
  // far-future timestamps so this case stays the newest 101 rows even if other
  // cases in the same process also wrote to Lobby
  const std::time_t base = 50'000'000'000;
  std::vector<std::string> ids;
  ids.reserve(101);
  for (int i = 0; i < 101; ++i) {
    const std::string id = unique("lim");
    ids.push_back(id);
    REQUIRE(database.saveMessage(makeStored(from, std::to_string(i), id, base + i), kLobby));
  }

  const auto history = database.loadHistory(kLobby);
  REQUIRE(history.size() == 100);

  std::unordered_set<std::string> returned;
  for (const Message &row : history) {
    returned.insert(row.getId());
  }
  REQUIRE_FALSE(returned.count(ids.front()));
  for (std::size_t i = 1; i < ids.size(); ++i) {
    REQUIRE(returned.count(ids[i]));
  }
  REQUIRE(history.front().getId() == ids.back());
}

// 15. online presence
TEST_CASE("DatabaseManager online presence", "[database_manager][online][edge]") {
  DatabaseManager &database = db();
  const std::string name = unique("online");

  REQUIRE_FALSE(database.isUserOnline(name));
  REQUIRE_FALSE(database.isUserOnline(""));

  database.setOnline(name, "node-a");
  REQUIRE(database.isUserOnline(name));
  REQUIRE_FALSE(database.isUserOnline(name + "_x"));

  SECTION("upsert node id does not duplicate the row") {
    database.setOnline(name, "node-b");
    REQUIRE(database.isUserOnline(name));
    database.clearOnline(name);
    REQUIRE_FALSE(database.isUserOnline(name));
  }

  SECTION("clear missing is a no-op") {
    database.clearOnline(unique("offline"));
    REQUIRE_FALSE(database.isUserOnline(unique("still-offline")));
  }

  SECTION("empty username can be marked online") {
    database.setOnline("", "node-a");
    REQUIRE(database.isUserOnline(""));
    database.clearOnline("");
    REQUIRE_FALSE(database.isUserOnline(""));
  }

  SECTION("unicode username") {
    const std::string unicode = unique("נוכח");
    database.setOnline(unicode, "node-a");
    REQUIRE(database.isUserOnline(unicode));
    database.clearOnline(unicode);
    REQUIRE_FALSE(database.isUserOnline(unicode));
  }
}

// 16. membership no-throw edges
TEST_CASE("DatabaseManager membership edges", "[database_manager][membership][edge]") {
  DatabaseManager &database = db();
  const std::string name = unique("member");

  REQUIRE_NOTHROW(database.setMembership(name, kLobby, "node-a"));
  REQUIRE_NOTHROW(database.setMembership(name, kLobby, "node-b"));
  REQUIRE_NOTHROW(database.setMembership(name, unique("room"), "node-a"));
  REQUIRE_NOTHROW(database.setMembership("", "", ""));
  REQUIRE_NOTHROW(database.clearMembership(name, unique("missing-room")));
  REQUIRE_NOTHROW(database.clearMembership(unique("nobody"), kLobby));
  REQUIRE_NOTHROW(database.clearAllMembership(name));
  REQUIRE_NOTHROW(database.clearAllMembership(unique("nobody")));
  REQUIRE_NOTHROW(database.clearAllMembership(""));

  const std::string unicode = unique("חבר");
  REQUIRE_NOTHROW(database.setMembership(unicode, kLobby, "צומת"));
  REQUIRE_NOTHROW(database.clearMembership(unicode, kLobby));
}

// 17. concurrent writes
TEST_CASE("DatabaseManager concurrent writes", "[database_manager][thread]") {
  DatabaseManager &database = db();
  const User from = createUniqueUser();
  constexpr int kWriters = 4;
  constexpr int kEach = 8;

  std::atomic<int> saved{0};
  std::atomic<int> onlineHits{0};
  std::vector<std::thread> threads;
  threads.reserve(kWriters * 2);

  for (int t = 0; t < kWriters; ++t) {
    threads.emplace_back([&database, &from, &saved, t]() {
      for (int i = 0; i < kEach; ++i) {
        const std::string id = unique("par");
        const Message msg = makeStored(from, "c", id, 9'800'000'000 + t * kEach + i);
        if (database.saveMessage(msg, kLobby)) {
          saved.fetch_add(1);
        }
      }
    });
    threads.emplace_back([&database, &onlineHits, t]() {
      const std::string name = unique("paron");
      database.setOnline(name, "node-" + std::to_string(t));
      if (database.isUserOnline(name)) {
        onlineHits.fetch_add(1);
      }
      database.clearOnline(name);
      REQUIRE_FALSE(database.isUserOnline(name));
    });
  }

  for (std::thread &thread : threads) {
    thread.join();
  }

  REQUIRE(saved.load() == kWriters * kEach);
  REQUIRE(onlineHits.load() == kWriters);
}

// 18. typical register / login / logout flow
TEST_CASE("DatabaseManager typical register login logout flow", "[database_manager][flow]") {
  DatabaseManager &database = db();
  const std::string name = unique("flow");
  const std::string password = "flow-secret";

  REQUIRE_FALSE(database.userExists(name));
  const User created = database.createUser(name, password, name + "@mail.test");
  REQUIRE(database.userExists(name));

  const auto login = database.loginUser(name, password);
  REQUIRE(std::holds_alternative<User>(login));

  database.setOnline(name, config::NODE_ID);
  database.setMembership(name, kLobby, config::NODE_ID);
  REQUIRE(database.isUserOnline(name));

  const Message msg = makeStored(created, "hello lobby", unique("flowmsg"), 9'900'000'000);
  REQUIRE(database.saveMessage(msg, kLobby));

  const auto history = database.loadHistory(kLobby);
  const auto it = std::find_if(history.begin(), history.end(),
                               [&](const Message &row) { return row.getId() == msg.getId(); });
  REQUIRE(it != history.end());
  REQUIRE(it->getFrom().getUsername() == name);

  database.clearOnline(name);
  database.clearAllMembership(name);
  REQUIRE_FALSE(database.isUserOnline(name));
  REQUIRE(database.userExists(name));
}
