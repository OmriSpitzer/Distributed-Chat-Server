SELECT username, email, user_type
FROM users 
WHERE username = ? AND password = ?;