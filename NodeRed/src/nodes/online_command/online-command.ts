/* eslint-disable no-param-reassign */
/* eslint-disable camelcase */
import type { Node, NodeInitializer } from "node-red";
import type { OnlineCommandConfig } from "../../@types/node_types";
import { runUserFunction } from "../common_server/runUserFunction";

const OnlineCommandNode: NodeInitializer = (RED): void => {
    function OnlineCommand(this: Node, config: OnlineCommandConfig): void {
        RED.nodes.createNode(this, config);
        this.on("input", async (message, send, done) => {
            // Get data from the first node, append our own
            // and pass it along if theres another node in the chain
            // if there is not, we should send it to CanRed

            let returnOutput = config.wires[0].length > 0;
            let userFunctionOutput: Awaited<ReturnType<typeof runUserFunction>>;

            try {
                userFunctionOutput = await runUserFunction(config.module_name, config.module_function, returnOutput);
                console.log(`userFunctionOutput: ${userFunctionOutput}`);
            } catch (e) {
                console.log(`An error occurred in online event!\n${e}`);
                // We failed, send over nothing.
                returnOutput = false;
            }

            if (returnOutput) {
                message.payload = userFunctionOutput;
            } else {
                message.payload = "";
            }
            console.log(`Sending message.payload = ${message.payload}`);
            send(message);
            if (done) {
                done();
            }
        });
    }

    RED.nodes.registerType("online-command", OnlineCommand);
};

export = OnlineCommandNode;
