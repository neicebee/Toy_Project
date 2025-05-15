create table if not exists users (
    id integer primary key autoincrement,
    username text not null unique,
    password_hash text not null,
    created_at datatime default current_timestamp
);