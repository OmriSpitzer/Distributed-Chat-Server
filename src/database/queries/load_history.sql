SELECT m.id, m.sender, m.content, m.created_at,
       COALESCE(u.email, ''), COALESCE(u.user_type, 'GUEST')
FROM messages m
LEFT JOIN users u ON u.username = m.sender
WHERE m.room = ?
ORDER BY m.created_at DESC
LIMIT 100;