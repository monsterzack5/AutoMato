import type { NodeInitializer, Node } from "node-red";
import type { OfflineEventConfig } from "../../@types/node_types";

const OfflineEventNode: NodeInitializer = (RED): void => {
    function OfflineEvent(this: Node, config: OfflineEventConfig): void {
        RED.nodes.createNode(this, config);
    }
    RED.nodes.registerType("offline-event", OfflineEvent);
};

export = OfflineEventNode;
