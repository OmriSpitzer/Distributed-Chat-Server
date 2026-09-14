INSERT OR IGNORE INTO messages (id, room_id, sender_username, sender_email, content, created_at, origin)
VALUES (?, ?, ?, ?, ?, ?, ?);
