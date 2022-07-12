import type {
    OfflineEventNode, FilteredFlowsFile, OfflineCommandNode, OfflineIfNode,
} from "../@types/flow_parsing_types";

// TODO:  These functions only run some basic tests.
// in the future, we should do this handling inside,
// of parseNodesImpl to further validate that flows are correct.

function peekNodeTypeByID(id: string, allNodes: FilteredFlowsFile): string | undefined {
    return allNodes.find((e) => e.id === id)?.type;
}

export function isValidEventNode(node: OfflineEventNode, allNodes: FilteredFlowsFile): boolean {
    // node is disabled
    if (node.d && node.d === true) {
        return false;
    }

    // No Children / Too many children
    if (node.wires[0].length !== 1) {
        return false;
    }

    const connectedNode = peekNodeTypeByID(node.wires[0][0], allNodes);

    // Child does not exist
    if (!connectedNode) {
        return false;
    }
    return true;
}

export function isValidCommandNode(node: OfflineCommandNode): boolean {
    // node is disabled
    if (node.d && node.d === true) {
        return false;
    }

    // Too many children
    if (node.wires[0].length > 1) {
        return false;
    }

    return true;
}

export function isValidIfNode(node: OfflineIfNode, allNodes: FilteredFlowsFile): boolean {
    // node is disabled
    if (node.d && node.d === true) {
        return false;
    }

    // No Children
    if (node.wires[0].length === 0 && node.wires[1].length === 0) {
        return false;
    }

    // Too many children
    if (node.wires[0].length > 1 || node.wires[1].length > 1) {
        return false;
    }

    const childNodes = node.wires.map((idArray) => peekNodeTypeByID(idArray[0], allNodes));

    // None of the children exist
    if (!childNodes.length) {
        return false;
    }

    return true;
}
