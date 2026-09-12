/**
 * Message Class unit tests
 *
 * @brief Includes: constructor fields, content edges, timestamp, unique ids, id format,
 * reconstruct ctor, equality, ordering, stream output, user-type edges, self-message,
 * stored user independence.
 * @date 12-09-2026
 */

#include "config/config.h"
#include "utils/models/message.h"
#include "utils/models/user.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <ctime>
#include <sstream>
#include <string>
#include <unordered_set>

/**
 * 1. constructor stores sender, receiver, content
 * 2. content edge cases
 * 3. timestamp is set near construction time
 * 4. successive messages receive unique ids
 * 5. id format includes node, boot, time, seq
 * 6. reconstruct ctor keeps stored id and timestamp
 * 7. equality and inequality
 * 8. ordering operators
 * 9. same-timestamp different-id ordering
 * 10. stream output
 * 11. user type and anonymous edges
 * 12. self-message and stored user independence
 */

// global users
static const User kAlice("alice", "alice@example.com", User::UserType::USER);
static const User kBob("bob", "bob@example.com", User::UserType::ADMIN);
static const User kGuest("guest", "guest@example.com", User::UserType::GUEST);
static const User kEmpty("", "", User::UserType::GUEST);
static const User kSpaced("  alice  ", "  a@b.com  ", User::UserType::USER);
static const User kUnicode("עֹמְרִי", "omri@example.com", User::UserType::USER);
static const User kAnon = User::anonymousUser();

// 1. constructor stores sender, receiver, content
TEST_CASE("Message constructor stores sender, receiver, and content", "[message][ctor]") {
  SECTION("plain text") {
    const Message msg(kAlice, kBob, "hello");
    REQUIRE(msg.getFrom() == kAlice);
    REQUIRE(msg.getTo() == kBob);
    REQUIRE(msg.getContent() == "hello");
    REQUIRE_FALSE(msg.getId().empty());
  }

  SECTION("admin to guest") {
    const Message msg(kBob, kGuest, "welcome");
    REQUIRE(msg.getFrom() == kBob);
    REQUIRE(msg.getTo() == kGuest);
    REQUIRE(msg.getContent() == "welcome");
  }

  SECTION("anonymous sender") {
    const Message msg(kAnon, kAlice, "hi");
    REQUIRE(msg.getFrom() == kAnon);
    REQUIRE(msg.getTo() == kAlice);
    REQUIRE(msg.getFrom().getUserType() == User::UserType::GUEST);
  }
}

// 2. content edge cases
TEST_CASE("Message content edge cases", "[message][ctor][edge]") {
  SECTION("empty content") {
    const Message msg(kAlice, kBob, "");
    REQUIRE(msg.getContent().empty());
    REQUIRE_FALSE(msg.getId().empty());
  }

  SECTION("whitespace only") {
    const Message msg(kAlice, kBob, "   \t  ");
    REQUIRE(msg.getContent() == "   \t  ");
  }

  SECTION("leading and trailing spaces") {
    const Message msg(kAlice, kBob, "  hello  ");
    REQUIRE(msg.getContent() == "  hello  ");
  }

  SECTION("multiline content") {
    const Message msg(kAlice, kBob, "line1\nline2\r\nline3");
    REQUIRE(msg.getContent() == "line1\nline2\r\nline3");
  }

  SECTION("unicode content") {
    const Message msg(kAlice, kBob, "שלום \xCE\xB1\xCE\xB2\xCE\xB3");
    REQUIRE(msg.getContent() == "שלום \xCE\xB1\xCE\xB2\xCE\xB3");
  }

  SECTION("special characters") {
    const std::string special = "a|b{c}\"d'\\e<>&;%@#";
    const Message msg(kAlice, kBob, special);
    REQUIRE(msg.getContent() == special);
  }

  SECTION("embedded null byte") {
    const std::string withNull("hello\0world", 11);
    const Message msg(kAlice, kBob, withNull);
    REQUIRE(msg.getContent() == withNull);
    REQUIRE(msg.getContent().size() == 11);
  }

  SECTION("long content") {
    const std::string longContent(100000, 'x');
    const Message msg(kAlice, kBob, longContent);
    REQUIRE(msg.getContent() == longContent);
    REQUIRE(msg.getContent().size() == 100000);
  }

  SECTION("single character") {
    const Message msg(kAlice, kBob, "x");
    REQUIRE(msg.getContent() == "x");
  }
}

// 3. timestamp is set near construction time
TEST_CASE("Message timestamp is set near construction time", "[message][timestamp]") {
  const auto before = std::time(nullptr);
  const Message msg(kAlice, kBob, "ping");
  const auto after = std::time(nullptr);

  REQUIRE(msg.getTimestamp() >= before);
  REQUIRE(msg.getTimestamp() <= after);
}

// 4. successive messages receive unique ids
TEST_CASE("Successive messages receive unique ids", "[message][id]") {
  SECTION("two messages") {
    const Message first(kAlice, kBob, "one");
    const Message second(kAlice, kBob, "two");

    REQUIRE(first.getId() != second.getId());
    REQUIRE(first != second);
    REQUIRE_FALSE(first == second);
  }

  SECTION("many messages") {
    std::unordered_set<std::string> ids;
    for (int i = 0; i < 200; ++i) {
      const Message msg(kAlice, kBob, "n" + std::to_string(i));
      REQUIRE(ids.insert(msg.getId()).second);
    }
    REQUIRE(ids.size() == 200);
  }

  SECTION("same content still unique ids") {
    const Message a(kAlice, kBob, "same");
    const Message b(kAlice, kBob, "same");
    REQUIRE(a.getContent() == b.getContent());
    REQUIRE(a.getId() != b.getId());
  }
}

// 5. id format includes node, boot, time, seq
TEST_CASE("Message id format includes node boot time and seq", "[message][id][format]") {
  const Message msg(kAlice, kBob, "format");
  const std::string &id = msg.getId();

  REQUIRE(id.find(config::NODE_ID + "-") == 0);

  // nodeId-bootId-unixTime-seq → at least 3 separators
  REQUIRE(std::count(id.begin(), id.end(), '-') >= 3);

  // id embeds the same timestamp used for the message
  REQUIRE(id.find("-" + std::to_string(msg.getTimestamp()) + "-") != std::string::npos);
}

// 6. reconstruct ctor keeps stored id and timestamp
TEST_CASE("Message reconstruct ctor keeps stored id and timestamp",
          "[message][ctor][reconstruct]") {
  SECTION("typical stored row") {
    const std::string storedId = "node1-boot-100-7";
    const std::time_t storedTs = 1700000000;
    const Message msg(kAlice, kBob, "from db", storedId, storedTs);

    REQUIRE(msg.getFrom() == kAlice);
    REQUIRE(msg.getTo() == kBob);
    REQUIRE(msg.getContent() == "from db");
    REQUIRE(msg.getId() == storedId);
    REQUIRE(msg.getTimestamp() == storedTs);
  }

  SECTION("empty id") {
    const Message msg(kAlice, kBob, "x", "", 0);
    REQUIRE(msg.getId().empty());
    REQUIRE(msg.getTimestamp() == 0);
  }

  SECTION("empty content and guest to") {
    const Message msg(kBob, kEmpty, "", "evt-1", 42);
    REQUIRE(msg.getContent().empty());
    REQUIRE(msg.getTo() == kEmpty);
    REQUIRE(msg.getId() == "evt-1");
    REQUIRE(msg.getTimestamp() == 42);
  }

  SECTION("does not allocate a new generated id") {
    const Message live(kAlice, kBob, "live");
    const Message restored(kAlice, kBob, "live", "fixed-id", live.getTimestamp());
    REQUIRE(restored.getId() == "fixed-id");
    REQUIRE(restored.getId() != live.getId());
  }

  SECTION("unicode users and content from storage") {
    const Message msg(kUnicode, kSpaced, "שלום", "id-עברית", 99);
    REQUIRE(msg.getFrom() == kUnicode);
    REQUIRE(msg.getTo() == kSpaced);
    REQUIRE(msg.getContent() == "שלום");
    REQUIRE(msg.getId() == "id-עברית");
  }
}

// 7. equality and inequality
TEST_CASE("Message equality compares id and timestamp", "[message][equality]") {
  SECTION("message equals itself") {
    const Message msg(kAlice, kBob, "same");
    REQUIRE(msg == msg);
    REQUIRE_FALSE(msg != msg);
  }

  SECTION("different generated messages are not equal") {
    const Message a(kAlice, kBob, "x");
    const Message b(kAlice, kBob, "x");
    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
  }

  SECTION("reconstructed copies with same id and timestamp are equal") {
    const Message a(kAlice, kBob, "body", "shared-id", 123);
    const Message b(kGuest, kEmpty, "other body", "shared-id", 123);
    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
  }

  SECTION("same id different timestamp are not equal") {
    const Message a(kAlice, kBob, "x", "id", 1);
    const Message b(kAlice, kBob, "x", "id", 2);
    REQUIRE(a != b);
  }

  SECTION("same timestamp different id are not equal") {
    const Message a(kAlice, kBob, "x", "id-a", 50);
    const Message b(kAlice, kBob, "x", "id-b", 50);
    REQUIRE(a != b);
  }
}

// 8. ordering operators
TEST_CASE("Message ordering is by timestamp", "[message][ordering]") {
  const Message earlier(kAlice, kBob, "old", "a", 10);
  const Message later(kAlice, kBob, "new", "b", 20);

  REQUIRE(earlier < later);
  REQUIRE(later > earlier);
  REQUIRE(earlier <= later);
  REQUIRE(later >= earlier);
  REQUIRE_FALSE(later < earlier);
  REQUIRE_FALSE(earlier > later);
  REQUIRE_FALSE(later <= earlier);
  REQUIRE_FALSE(earlier >= later);
}

// 9. same-timestamp different-id ordering
TEST_CASE("Message same-timestamp different-id ordering", "[message][ordering][edge]") {
  const Message a(kAlice, kBob, "one", "id-a", 100);
  const Message b(kAlice, kBob, "two", "id-b", 100);

  REQUIRE_FALSE(a < b);
  REQUIRE_FALSE(a > b);
  REQUIRE_FALSE(b < a);
  REQUIRE_FALSE(b > a);

  // <= / >= require < or ==; same timestamp + different id ⇒ neither
  REQUIRE_FALSE(a == b);
  REQUIRE_FALSE(a <= b);
  REQUIRE_FALSE(a >= b);
  REQUIRE_FALSE(b <= a);
  REQUIRE_FALSE(b >= a);

  SECTION("self ordering") {
    REQUIRE_FALSE(a < a);
    REQUIRE_FALSE(a > a);
    REQUIRE(a <= a);
    REQUIRE(a >= a);
  }
}

// 10. stream output
TEST_CASE("Message stream output includes users content id and timestamp", "[message][print]") {
  SECTION("plain text") {
    const Message msg(kAlice, kBob, "hello");
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("alice") != std::string::npos);
    REQUIRE(text.find("bob") != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
    REQUIRE(text.find(msg.getId()) != std::string::npos);
    REQUIRE(text.find(std::to_string(msg.getTimestamp())) != std::string::npos);
  }

  SECTION("empty content") {
    const Message msg(kAlice, kBob, "");
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("alice") != std::string::npos);
    REQUIRE(text.find("bob") != std::string::npos);
    REQUIRE(text.find(msg.getId()) != std::string::npos);
    REQUIRE(text.find("Content:") != std::string::npos);
  }

  SECTION("multiline content") {
    const Message msg(kAlice, kBob, "line1\nline2");
    std::ostringstream out;
    out << msg;
    REQUIRE(out.str().find("line1\nline2") != std::string::npos);
  }

  SECTION("reconstructed message") {
    const Message msg(kGuest, kEmpty, "hist", "hist-id", 55);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("guest") != std::string::npos);
    REQUIRE(text.find("hist-id") != std::string::npos);
    REQUIRE(text.find("55") != std::string::npos);
    REQUIRE(text.find("hist") != std::string::npos);
  }

  SECTION("unicode users") {
    const Message msg(kUnicode, kAlice, "msg");
    std::ostringstream out;
    out << msg;
    REQUIRE(out.str().find("עֹמְרִי") != std::string::npos);
  }
}

// 11. user type and anonymous edges
TEST_CASE("Message works with all user types and empty users", "[message][users][edge]") {
  SECTION("empty username users") {
    const Message msg(kEmpty, kEmpty, "nobody");
    REQUIRE(msg.getFrom().getUsername().empty());
    REQUIRE(msg.getTo().getUsername().empty());
  }

  SECTION("spaced usernames preserved") {
    const Message msg(kSpaced, kBob, "hi");
    REQUIRE(msg.getFrom().getUsername() == "  alice  ");
  }

  SECTION("anonymous to anonymous") {
    const User anon2 = User::anonymousUser();
    const Message msg(kAnon, anon2, "whisper");
    REQUIRE(msg.getFrom().getUserType() == User::UserType::GUEST);
    REQUIRE(msg.getTo().getUserType() == User::UserType::GUEST);
    REQUIRE(msg.getContent() == "whisper");
  }
}

// 12. self-message and stored user independence
TEST_CASE("Message self-send and stores independent user copies", "[message][ctor][edge]") {
  SECTION("message to self") {
    const Message msg(kAlice, kAlice, "note to self");
    REQUIRE(msg.getFrom() == msg.getTo());
    REQUIRE(msg.getFrom() == kAlice);
    REQUIRE(msg.getContent() == "note to self");
  }

  SECTION("mutating source user after construction does not change message") {
    User mutableFrom("tmp", "tmp@example.com", User::UserType::USER);
    const Message msg(mutableFrom, kBob, "snap");
    mutableFrom.setUsername("changed");
    mutableFrom.setEmail("changed@example.com");

    REQUIRE(msg.getFrom().getUsername() == "tmp");
    REQUIRE(msg.getFrom().getEmail() == "tmp@example.com");
    REQUIRE(msg.getFrom() != mutableFrom);
  }

  SECTION("getContent and getId return stable references to stored fields") {
    const Message msg(kAlice, kBob, "ref");
    const std::string &contentRef = msg.getContent();
    const std::string &idRef = msg.getId();
    REQUIRE(&contentRef == &msg.getContent());
    REQUIRE(&idRef == &msg.getId());
  }
}
