/* eslint-disable no-param-reassign */

import { readFileSync } from "fs";
import type {
    AnyChildNode, FilteredFlowsFile, NodeRedNode, OfflineEventNode, EventFlow,
} from "../@types/flow_parsing_types";
import { getFormattedCommand, getFormattedIf, getFormattedEvent } from "./formatNodes";
import { isFlowValid } from "./util";
import { isValidEventNode, isValidCommandNode, isValidIfNode } from "./validateNodes";

function getMaxSectionNumber(flows: EventFlow): number {
    return flows.reduce((acc: number, key) => {
        if (key.section_number > acc) {
            // eslint-disable-next-line no-param-reassign
            acc = key.section_number;
        }
        return acc;
    }, 0);
}

function getNextNodeByID(id: string, nodes: AnyChildNode[]): AnyChildNode | undefined {
    return nodes.find((e) => (e ? e.id === id : undefined));
}

function parseNodesImpl(nodes: AnyChildNode[], flowSoFar: EventFlow, nextNodeID: string, sectionNumber = 1): EventFlow {
    if (flowSoFar.length === 0) {
        // This should not be possible!
        return flowSoFar;
    }

    const nextNode = getNextNodeByID(nextNodeID, nodes);

    if (!nextNode) {
        // We're given a nextNodeID, but that ID does not exist in the flow.
        return flowSoFar;
    }

    if (nextNode.type === "offline-command") {
        // nextSection points to the command after this one.
        const nextSection = getNextNodeByID(nextNode.wires[0][0], nodes) ? sectionNumber + 1 : 0;
        flowSoFar.push(getFormattedCommand(nextNode, sectionNumber, nextSection));

        if (nextSection) {
            // Parse the node after us.
            sectionNumber += 1;
            return parseNodesImpl(nodes, flowSoFar, nextNode.wires[0][0], sectionNumber);
        }

        return flowSoFar;
    }

    if (nextNode.type === "offline-if") {
        // we don't append IF's till after we parse both legs
        // so we need to cache the current section number
        // and add 1 to it, for the next node we parse.
        const ifSectionNumber = sectionNumber;
        sectionNumber += 1;

        const trueLegNodeID = nextNode.wires[0][0];
        let trueLegSectionNumber = 0;

        const falseLegNodeID = nextNode.wires[1][0];
        let falseLegSectionNumber = 0;

        if (trueLegNodeID) {
            trueLegSectionNumber = sectionNumber;
            flowSoFar = parseNodesImpl(nodes, flowSoFar, trueLegNodeID, sectionNumber);
            // we need a new, unused section number, so we get the max section number, and add 1
            sectionNumber = getMaxSectionNumber(flowSoFar) + 1;
        }

        if (falseLegNodeID) {
            falseLegSectionNumber = sectionNumber;
            flowSoFar = parseNodesImpl(nodes, flowSoFar, falseLegNodeID, sectionNumber);
        }

        flowSoFar.push(getFormattedIf(nextNode, ifSectionNumber, trueLegSectionNumber, falseLegSectionNumber));
        return flowSoFar;
    }

    return flowSoFar;
}

function parseNodes(eventNode: OfflineEventNode, nodes: AnyChildNode[]): EventFlow {
    const flow: EventFlow = [getFormattedEvent(eventNode)];
    return parseNodesImpl(nodes, flow, eventNode.wires[0][0]);
}

function splitUpNodes(allFlows: FilteredFlowsFile): { eventNodes: OfflineEventNode[], childNodes: AnyChildNode[] } {
    const eventNodes: OfflineEventNode[] = [];
    const childNodes: AnyChildNode[] = [];

    // Collect all the nodes, and check if they're valid
    for (const node of allFlows) {
        const { type } = node;
        if (type === "offline-event" && isValidEventNode(node, allFlows)) {
            eventNodes.push(node);
        } else
        if (type === "offline-command" && isValidCommandNode(node)) {
            childNodes.push(node);
        } else
        if (type === "offline-if" && isValidIfNode(node, allFlows)) {
            childNodes.push(node);
        }
    }
    return { eventNodes, childNodes };
}

export function parseFlows(filePath: string | null = null): EventFlow[] {
    // when called from the cli, we pass this function a full file path
    // when called from express, we're expected to use the userDir/flows.json
    const flowsFilePath = filePath ?? `${process.env.userDir}/flows.json`;
    let filteredNodes: FilteredFlowsFile;
    try {
        const flows = readFileSync(flowsFilePath, "utf-8").toString();
        filteredNodes = JSON.parse(flows).filter((node: NodeRedNode) => node.type.startsWith("offline-"));
    } catch (e) {
        console.log(`Error Parsing Flows File!\n${e}`);
        return [];
    }

    const { eventNodes, childNodes } = splitUpNodes(filteredNodes);

    // For every Event Node, parse it's flows.
    // Filter out any flows that have a length of 1,
    // which are invalid flows that only contain an Event Node.
    const parsedNodes = eventNodes.map((node) => parseNodes(node, childNodes))
        .filter((node) => node.length > 1);

    for (const node of parsedNodes) {
        if (!isFlowValid(node)) {
            console.log("Error while parsing flows");
            return [];
        }
    }

    return parsedNodes;
}
