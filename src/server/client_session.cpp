/**
 * ClientSession class
 *
 * @brief Holds the per-connection state for a single client.
 * @date 06-09-2026
 */

#include "server/client_session.h"

// constructor
ClientSession::ClientSession(int socket, const User &user, const Room &room)
    : clientSocket(socket), user(user), authenticated(false), currentRoom(room) {}

// getters
int ClientSession::getSocket() const { return clientSocket; }
const User &ClientSession::getUser() const { return user; }
const Room &ClientSession::getRoom() const { return currentRoom; }
bool ClientSession::isAuthenticated() const { return authenticated; }

// setters
void ClientSession::setUser(const User &newUser) { user = newUser; }
void ClientSession::setRoom(const Room &newRoom) { currentRoom = newRoom; }
void ClientSession::setAuthenticated(bool value) { authenticated = value; }
