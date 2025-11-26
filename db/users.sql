CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    token_id UUID NOT NULL,
    created_at TIMESTAMP NOT NULL
);