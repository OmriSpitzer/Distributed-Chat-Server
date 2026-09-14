SELECT m.id, m.sender_username, m.content, m.created_at,
       m.sender_email, COALESCE(u.user_type, 'GUEST')
FROM messages m
LEFT JOIN users u ON u.email = m.sender_email
WHERE m.room_id = ?
ORDER BY m.created_at DESC
LIMIT 100;
