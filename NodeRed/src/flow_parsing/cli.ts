import { existsSync } from "fs";
import { parseFlows } from "./parseNodes";
import { printFlows } from "./util";

async function main(): Promise<void> {
    if (!process.argv[2]) {
        console.log("expected input:\n node cli.js <path to flows file>");
        return;
    }

    if (!existsSync(process.argv[2])) {
        console.log("Error: flows file does not exist");
        return;
    }

    const parsedFlows = parseFlows(process.argv[2]);

    if (parsedFlows.length === 0) {
        console.log("Nothing to parse!");
    } else {
        printFlows(parsedFlows);
    }
}

main();
