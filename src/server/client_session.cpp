/**
 * ClientSession class
 *
 * @brief Holds the per-connection state for a single client.
 * @date 14-07-2026
 */

#include "server/client_session.h"

ClientSession::ClientSession(int socket, const User &user, const Room &room)
    : clientSocket(socket), user(user), authenticated(false), currentRoom(room) {}

int ClientSession::getSocket() const { return clientSocket; }
const User &ClientSession::getUser() const { return user; }
bool ClientSession::isAuthenticated() const { return authenticated; }

void ClientSession::setUser(const User &newUser) { user = newUser; }
void ClientSession::setAuthenticated(bool value) { authenticated = value; }
