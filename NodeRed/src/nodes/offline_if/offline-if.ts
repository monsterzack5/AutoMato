import type { Node, NodeInitializer } from "node-red";
import type { OfflineEventConfig } from "../../@types/node_types";
// import type { AutomatoSecondaryEvent, OfflineEventConfig, OfflineNodeOutput } from "../../@types/types";

const OfflineIfNode: NodeInitializer = (RED): void => {
    function OfflineIf(this: Node, config: OfflineEventConfig): void {
        RED.nodes.createNode(this, config);
        // NO-OP
    }

    RED.nodes.registerType("offline-if", OfflineIf);
};

export = OfflineIfNode;
