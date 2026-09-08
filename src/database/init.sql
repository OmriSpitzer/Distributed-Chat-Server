-- Create users table
CREATE TABLE IF NOT EXISTS users (
  username  TEXT PRIMARY KEY,
  email     TEXT NOT NULL,
  password  TEXT NOT NULL,
  user_type ENUM('USER', 'ADMIN') NOT NULL
);

-- Create rooms table
CREATE TABLE IF NOT EXISTS rooms (
  name     TEXT PRIMARY KEY,
  type     ENUM('RESEARCH_AND_DEVELOPMENT', 'PRODUCTION', 'QA', 'DEVOPS', 'SECURITY', 'DESIGN', 'MARKETING', 'HR', 'FINANCE', 'LEGAL', 'CUSTOMER_SUPPORT', 'OTHER', 'LOBBY') NOT NULL,
  privacy  ENUM('PUBLIC', 'PRIVATE') NOT NULL
);

-- Create messages table
CREATE TABLE IF NOT EXISTS messages (
  id         TEXT PRIMARY KEY,  
  room       TEXT NOT NULL,
  sender     TEXT NOT NULL,
  content    TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  origin     TEXT NOT NULL      
);

-- Create membership table
CREATE TABLE IF NOT EXISTS membership (
  username TEXT NOT NULL,
  room     TEXT NOT NULL,
  node_id  TEXT NOT NULL,
  PRIMARY KEY (username, room)
);

-- Populate rooms table
INSERT OR IGNORE INTO rooms (name, type, privacy)
VALUES ('Lobby', 'LOBBY', 'PUBLIC');

-- Populate users table
INSERT OR IGNORE INTO users (username, email, password, user_type) VALUES
  ('omri', 'omri@gmail.com', '123', 'USER'),
  ('spitzer', 'spitzer@gmail.com', '123', 'USER'),
  ('admin', 'admin@gmail.com', '123', 'ADMIN');
