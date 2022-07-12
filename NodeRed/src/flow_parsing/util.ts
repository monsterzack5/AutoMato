/* eslint-disable camelcase */
import { EventFlow } from "../@types/flow_parsing_types";

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export function areTheSame(a: any, b: any): boolean {
    const aKeys = Object.keys(a);
    const bKeys = Object.keys(b);

    if (aKeys.length !== bKeys.length) {
        return false;
    }

    for (const key of aKeys) {
        if (a[key] !== b[key]) {
            return false;
        }
    }
    return true;
}

function areThereDuplicateSectionNumbers(flow: EventFlow): boolean {
    // Section numbers should be unique, we should never have duplicates.

    const allSectionNumbers = new Set();

    let anyDuplicates = false;
    for (const node of flow) {
        const { section_number } = node;
        if (allSectionNumbers.has(section_number)) {
            console.log(`Duplicate Section Number Found! Number: ${section_number}`);
            anyDuplicates = true;
        } else allSectionNumbers.add(section_number);
    }
    return anyDuplicates;
}
function areThereMissingSectionNumbers(flows: EventFlow): boolean {
    // Section Numbers should always be one after another, we should
    // never have a missing one.
    let anyMissingNumbers = false;

    // Create a copy so we don't mutate the flows Object
    const flowsCopy = [...flows];

    flowsCopy.sort((a, b) => a.section_number - b.section_number);

    for (let i = 0; i < flowsCopy.length; i += 1) {
        if (flowsCopy[i].section_number !== i) {
            console.log(`Missing Section Number! Number: ${i}`);
            anyMissingNumbers = true;
        }
    }
    return anyMissingNumbers;
}

export function isFlowValid(flow: EventFlow): boolean {
    // I am not sure if JS short circuits, So we call these functions
    // individually
    const duplicates = areThereDuplicateSectionNumbers(flow);
    const missingNumbers = areThereMissingSectionNumbers(flow);
    return !(duplicates || missingNumbers);
}

export function printFlows(flows: EventFlow[]): void {
    if (flows.length === 1) {
        const sorted = flows[0].sort((a, b) => a.section_number - b.section_number);
        console.log(`${JSON.stringify(sorted, null, 2)}`);
        isFlowValid(flows[0]);
        return;
    }

    for (let i = 0; i < flows.length; i += 1) {
        const sorted = flows[i].sort((a, b) => a.section_number - b.section_number);
        console.log(`Flow #${i + 1}: ${JSON.stringify(sorted, null, 2)}\n`);
        isFlowValid(flows[i]);
        console.log(`\nEnd Flow #${i + 1}\n`);
    }
}
