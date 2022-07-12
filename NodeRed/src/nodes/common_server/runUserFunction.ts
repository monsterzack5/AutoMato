/* eslint-disable consistent-return */
import net from "net";
import type { IPCInput, IPCOutput } from "../../@types/ipc_types";

export enum OutputType {
    signed_int = 1,
    unsigned_int = 2,
    signed_bigint = 3,
    unsigned_bigint = 4,
    float = 5,
    double = 6,
    bool = 7,
    void = 8,
}

export async function runUserFunction(moduleName: string, moduleFunction: string, returnOutput: boolean): Promise<number | bigint | void> {
    // When the socket is closed, but we don't have enough data
    // to form a complete message, we call socket.emit(data, socketDataFailInput)
    // Which tells the
    const socketUnexpectedCloseInput = Buffer.from("FORCE-PARSE-OR-FAIL");

    return new Promise((resolve, reject) => {
        let socketRawReplyData = "";
        let socketReply: IPCInput = {};

        const socket = net.createConnection("/tmp/CanRed/red.sock");

        setTimeout(reject, 180000, Error("runUserFunction Timed out!")); // Wait 3 minutes for command to reply, or fail.

        socket.on("error", (e) => {
            console.log(`Failed to open a socket to CanRed!\n${e}`);
            socket.destroy();
            reject(e);
        });

        socket.on("close", () => {
            console.log("close");
            // The socket closed, but we didn't expect that.
            socket.emit("data", socketUnexpectedCloseInput);
        });

        socket.on("connect", () => {
            console.log("connect");
            const socketMessage: IPCOutput = {
                module_name: moduleName,
                module_function: moduleFunction,
                return_output: returnOutput,
            };

            socket.write(JSON.stringify(socketMessage));
        });

        socket.on("data", (chunk) => {
            console.log("data!");

            if (chunk === socketUnexpectedCloseInput) {
                // TODO: Ideally we should parse what we have, and hope it's valid.
                //       As maybe the sender failed to properly close the socket,
                //       but still send all it needed to send.
                return;
            }

            socketRawReplyData += chunk;

            try {
                socketReply = JSON.parse(socketRawReplyData);
                console.log(`Socket Reply: \n${JSON.stringify(socketReply, null, 2)}`);
            } catch (e) {
            // we have an error parsing the data.
            // lets assume we just haven't read the
            // full reply yet.
                if (chunk === socketUnexpectedCloseInput) {
                    reject(Error("Socket Closed Unexpectedly"));
                }
                return;
            }

            // At this point, we have parsed the output
            // and don't need the socket anymore.
            socket.destroy();

            if (returnOutput && socketReply.output && socketReply.output_type) {
                switch (socketReply.output_type) {
                    case OutputType.signed_int:
                    case OutputType.unsigned_int:
                        return resolve(parseInt(socketReply.output, 10));

                    case OutputType.signed_bigint:
                    case OutputType.unsigned_bigint:
                        return resolve(BigInt(socketReply.output));

                    case OutputType.float:
                    case OutputType.double:
                        return resolve(parseFloat(socketReply.output));

                    case OutputType.bool:
                        console.log("bool");
                        return resolve(parseInt(socketReply.output, 2));

                    case OutputType.void:
                        return resolve(undefined);

                    default:
                    // Parse error!
                        throw new Error("Unknown Output Type from CanRed Socket.");
                }
            }
        });
    });
}
