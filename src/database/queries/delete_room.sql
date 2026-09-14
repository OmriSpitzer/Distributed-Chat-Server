DELETE FROM membership WHERE room_id = ?;
DELETE FROM messages WHERE room_id = ?;
DELETE FROM rooms WHERE id = ?;
