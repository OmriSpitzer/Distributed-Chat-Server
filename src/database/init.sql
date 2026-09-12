-- Create users table
CREATE TABLE IF NOT EXISTS users (
  username  TEXT PRIMARY KEY,
  email     TEXT NOT NULL UNIQUE,
  password  TEXT NOT NULL,
  user_type TEXT NOT NULL
);

-- Create rooms table
CREATE TABLE IF NOT EXISTS rooms (
  name     TEXT PRIMARY KEY,
  type     TEXT NOT NULL,
  privacy  TEXT NOT NULL
);

-- Create messages table
CREATE TABLE IF NOT EXISTS messages (
  id         TEXT PRIMARY KEY,
  room       TEXT NOT NULL,
  sender     TEXT NOT NULL,
  content    TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  origin     TEXT NOT NULL,
  FOREIGN KEY (room) REFERENCES rooms(name),
  FOREIGN KEY (sender) REFERENCES users(username)
);

-- Create membership table
CREATE TABLE IF NOT EXISTS membership (
  username TEXT NOT NULL,
  room     TEXT NOT NULL,
  node_id  TEXT NOT NULL,
  PRIMARY KEY (username, room)
);

-- Cluster-wide presence (who is logged in, on which node)
CREATE TABLE IF NOT EXISTS online_users (
  username TEXT PRIMARY KEY,
  node_id  TEXT NOT NULL
);

-- Populate rooms table
INSERT OR IGNORE INTO rooms (name, type, privacy)
VALUES ('Lobby', 'LOBBY', 'PUBLIC');

-- Populate users table
INSERT OR IGNORE INTO users (username, email, password, user_type) VALUES
  ('omri', 'omri@gmail.com', '$argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>', 'USER'),
  ('spitzer', 'spitzer@gmail.com', '$argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>', 'USER'),
  ('admin', 'admin@gmail.com', '$argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>', 'ADMIN');
