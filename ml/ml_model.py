


import sqlite3

DB_PATH = "/home/sa25/Desktop/agri/data/users.db"  # <-- change this
OUTPUT_FILE = "db_documentation.txt"


def get_tables(cursor):
    cursor.execute("""
        SELECT name FROM sqlite_master 
        WHERE type='table' AND name NOT LIKE 'sqlite_%'
    """)
    return [row[0] for row in cursor.fetchall()]


def get_columns(cursor, table_name):
    cursor.execute(f"PRAGMA table_info({table_name})")
    return cursor.fetchall()


def format_table_doc(table_name, columns):
    doc = []
    doc.append(f"4.x Table Name: {table_name.capitalize()}")
    doc.append(f"Description: This table is used to store details of {table_name}.\n")

    doc.append("Column Name | Data Type | Size | Constraints | Description")
    doc.append("-" * 90)

    for col in columns:
        cid, name, col_type, notnull, default_value, pk = col

        constraints = []
        if pk:
            constraints.append("Primary Key")
        if notnull:
            constraints.append("Not Null")
        if default_value is not None:
            constraints.append(f"Default({default_value})")

        constraint_str = ", ".join(constraints) if constraints else "None"

        # SQLite doesn't enforce size strictly, so we approximate
        size = "N/A"

        description = f"{name.replace('_', ' ').title()}"

        doc.append(f"{name} | {col_type} | {size} | {constraint_str} | {description}")

    doc.append("\n")
    return "\n".join(doc)


def main():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    tables = get_tables(cursor)

    full_doc = []

    for table in tables:
        columns = get_columns(cursor, table)
        table_doc = format_table_doc(table, columns)
        full_doc.append(table_doc)

    conn.close()

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(full_doc))

    print(f"Documentation generated successfully -> {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
