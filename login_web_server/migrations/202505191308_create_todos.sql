create table if not exists todos (
    id integer primary key autoincrement,
    title text not null,
    created_at datetime not null default current_timestamp
);