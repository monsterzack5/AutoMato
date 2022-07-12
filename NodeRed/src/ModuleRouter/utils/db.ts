import Sqlite from "better-sqlite3";
import { existsSync } from "fs";
import type { Options } from "better-sqlite3";

class Database {
    private options: Options = { readonly: true };

    public db: Sqlite.Database;

    public constructor() {
        if (!existsSync(`${process.env.dbFile}`)) {
            console.log("Fatal Error: database file not found!\n");
            process.exit(1);
        }

        if (process.env.NODE_ENV === "dev") {
            this.options.verbose = console.log;
        }

        this.db = new Sqlite(`${process.env.dbFile}`, this.options);
    }
}

export const { db } = new Database();
