import { readFileSync, writeFileSync } from "fs";
import { randomUUID } from "crypto";

function generateNewSecret(): string {
    // Reads the .env file, splits it into an array from new lines
    // and sets credentialSecret to a randomUUID
    const file = readFileSync(".env", "utf-8").toString().split("\n");
    const index = file.findIndex((line) => line.startsWith("credentialSecret="));
    const newSecret = randomUUID();

    if (index >= 0) {
        file[index] = `credentialSecret=${newSecret}`;
    } else {
        // does not appear in the file.
        file.push(`credentialSecret=${newSecret}`);
    }

    writeFileSync(".env", file.join("\n"));

    console.log("INFO: Node-Red credentialSecret left blank in .env file, Autogenerating one!");
    console.log("INFO: It would be a good idea to backup this key!");
    return newSecret;
}

export function checkENVVars(): void {
    process.env.userDir ??= "/home/pi/.nodered";
    process.env.port ??= "1880";
    process.env.dbFile ??= "/home/pi/.automato/CanRed.db";
    process.env.credentialSecret ??= generateNewSecret();
}
