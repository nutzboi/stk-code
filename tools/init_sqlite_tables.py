#!/usr/bin/env python3
import os
import sqlite3
import sys


def _read_sql_file(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def main() -> int:
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sql_path = os.path.join(repo_root, "new_tables.txt")

    if not os.path.exists(sql_path):
        print(f"ERROR: SQL file not found: {sql_path}", file=sys.stderr)
        return 2

    print("Note: this applies only TierS-added tables (not upstream STK official tables).")
    db_path = input("Path to stkservers.db (SQLite file): ").strip()
    if not db_path:
        print("ERROR: No path provided.", file=sys.stderr)
        return 2

    if not os.path.exists(db_path):
        print(f"ERROR: DB file does not exist: {db_path}", file=sys.stderr)
        return 2

    sql = _read_sql_file(sql_path)

    try:
        con = sqlite3.connect(db_path)
        try:
            con.execute("PRAGMA foreign_keys = ON;")
            con.executescript(sql)
            con.commit()
        finally:
            con.close()
    except sqlite3.Error as e:
        print(f"ERROR: SQLite error: {e}", file=sys.stderr)
        return 1

    print("OK: Tables/indexes applied.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
