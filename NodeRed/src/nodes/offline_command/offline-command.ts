import type { Node, NodeInitializer } from "node-red";
import type { OfflineCommandConfig } from "../../@types/node_types";
// import type { AutomatoSecondaryEvent, OfflineEventConfig, OfflineNodeOutput } from "../../@types/types";

const OfflineCommandNode: NodeInitializer = (RED): void => {
    function OfflineCommand(this: Node, config: OfflineCommandConfig): void {
        RED.nodes.createNode(this, config);
        // NO-OP
    }

    RED.nodes.registerType("offline-command", OfflineCommand);
};

export = OfflineCommandNode;
