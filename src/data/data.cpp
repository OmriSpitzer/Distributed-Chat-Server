/*
 * Data class
 *
 * @date 07-09-2026
 */
#include "data/data.h"
#include <map>
#include <string>
#include <vector>

// stub users
std::vector<std::map<std::string, std::string>> Data::users = {
    // username, email, password, type
    {{"username", "omri"}, {"email", "omri@gmail.com"}, {"password", "123"}, {"type", "USER"}},
    {{"username", "spitzer"},
     {"email", "spitzer@gmail.com"},
     {"password", "123"},
     {"type", "USER"}},
    {{"username", "admin"}, {"email", "admin@gmail.com"}, {"password", "123"}, {"type", "ADMIN"}},
};

// stub rooms
std::vector<std::map<std::string, std::string>> Data::rooms = {
    // id, name
    {{"id", "1"}, {"name", "Room 1"}},
    {{"id", "2"}, {"name", "Room 2"}},
};

// stub messages
std::vector<std::map<std::string, std::string>> Data::messages = {
    // id, room id, from, to, content, timestamp
    {{"id", "1"},
     {"room_id", "1"},
     {"from", "spitzer"},
     {"to", "omri"},
     {"content", "Message 1"},
     {"timestamp", "1"}},
    {{"id", "2"},
     {"room_id", "2"},
     {"from", "omri"},
     {"to", "spitzer"},
     {"content", "Message 2"},
     {"timestamp", "2"}},
};