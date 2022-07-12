import { readFileSync, writeFileSync } from "fs";
import { EventFlow } from "../@types/flow_parsing_types";
import { parseFlows } from "./parseNodes";
import { areTheSame } from "./util";

function sortEventFlowArray(input: EventFlow[]): EventFlow[] {
    // input: [[], [] ....]
    // first, sort the outter array based on flow name
    // second, sort every inner array based on section number.
    input.sort((a, b) => a[0].flow_name.localeCompare(b[0].flow_name));
    for (let i = 0; i < input.length; i += 1) {
        input[i].sort((a, b) => a.section_number - b.section_number);
    }
    return input;
}

function didFlowsChange(newFlows: EventFlow[], oldFlows: EventFlow[]): boolean {
    // Do some simple tests to detect if the flows have changed.

    // Make sure the length of both of them is correct
    if (newFlows.length !== oldFlows.length) {
        return true;
    }

    // For every array, make sure the arrays are all the right size.
    for (let i = 0; i < newFlows.length; i += 1) {
        if (newFlows[i].length !== oldFlows[i].length) {
            return true;
        }

        // For every Node, check if their objects are the same.
        for (let j = 0; j < newFlows[i].length; j += 1) {
            if (!areTheSame(newFlows[i][j], oldFlows[i][j])) {
                return true;
            }
        }
    }

    return false;
}

export function updateFlowsIfNeeded(): void {
    const parsedFlowsFileLocation = `${process.env.userDir}/automato.parsed.flows.json`;

    const newParsedFlows = parseFlows();

    const sortedNewFlows = sortEventFlowArray(newParsedFlows);

    const flowsOnDisk: EventFlow[] = JSON.parse(readFileSync(parsedFlowsFileLocation, "utf-8").toString());

    if (didFlowsChange(sortedNewFlows, flowsOnDisk)) {
        writeFileSync(parsedFlowsFileLocation, JSON.stringify(newParsedFlows));
        console.log("Updating automato.parsed.flows.json");
    }
}
