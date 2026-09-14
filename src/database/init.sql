-- Create users table
CREATE TABLE IF NOT EXISTS users (
  username  TEXT NOT NULL UNIQUE,
  email     TEXT PRIMARY KEY,
  password  TEXT NOT NULL,
  user_type TEXT NOT NULL CHECK (user_type IN ('ADMIN', 'USER', 'GUEST'))
);

-- Create rooms table
CREATE TABLE IF NOT EXISTS rooms (
  id       INTEGER PRIMARY KEY AUTOINCREMENT,
  name     TEXT NOT NULL UNIQUE,
  type     TEXT NOT NULL CHECK (type IN 
  ('R&D', 'Production', 'QA', 'DevOps', 'Security', 'Design', 
  'Marketing', 'HR', 'Finance', 'Legal', 'Customer Support', 'Other', 'Lobby')),
  privacy  TEXT NOT NULL CHECK (privacy IN ('PUBLIC', 'PRIVATE'))
);

-- Create messages table
CREATE TABLE IF NOT EXISTS messages (
  id                  TEXT PRIMARY KEY,
  room_id             INTEGER NOT NULL,
  sender_username     TEXT NOT NULL,
  sender_email        TEXT NOT NULL,
  content             TEXT NOT NULL,
  created_at          INTEGER NOT NULL,
  origin              TEXT NOT NULL,
  FOREIGN KEY (room_id) REFERENCES rooms(id),
  FOREIGN KEY (sender_email) REFERENCES users(email)
);

-- Create membership table
CREATE TABLE IF NOT EXISTS membership (
  username TEXT NOT NULL,
  room_id  INTEGER NOT NULL,
  node_id  TEXT NOT NULL,
  PRIMARY KEY (username, room_id),
  FOREIGN KEY (username) REFERENCES users(username),
  FOREIGN KEY (room_id) REFERENCES rooms(id)
);

-- Cluster-wide presence (who is logged in, on which node)
CREATE TABLE IF NOT EXISTS online_users (
  username TEXT PRIMARY KEY,
  node_id  TEXT NOT NULL
);

-- Populate users table
INSERT OR IGNORE INTO users (username, email, password, user_type) VALUES
  ('omri', 'omri@gmail.com', '$argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>', 'USER'),
  ('spitzer', 'spitzer@gmail.com', '$argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>', 'USER'),
  ('admin', 'admin@gmail.com', '$argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>', 'ADMIN');

-- Populate rooms table
INSERT OR IGNORE INTO rooms (id, name, type, privacy) VALUES
  (1, 'Lobby', 'Lobby', 'PUBLIC'),
  (2, 'General', 'Other', 'PUBLIC');
