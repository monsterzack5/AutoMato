import type { EditorRED } from "node-red";
import type { OfflineEventRegisterType } from "../../@types/node_types";
import { updateModuleFunctionsAutofill, updateModulesAutofill } from "../common_browser/autofill";

declare const RED: EditorRED;
// TODO: Validate user input, maybe store the expected side of the
// command return value, like uint8_t, and then check that its inside
// of that size

// TODO: #node-input-value_to_check is passed as a string
// and I have absolutely no idea why. It's handled the exact same
// as #node-input-interval, which is passed correctly as a number

// This tells our express instance to parse and update the nodes.
RED.events.on("deploy", () => fetch("/automato/updateflows"));

RED.nodes.registerType<OfflineEventRegisterType>("offline-event", {
    category: "Automato",
    color: "#a6bbcf",
    // TODO: Validate
    defaults: {
        flow_name: { value: "", required: true },
        module_name: { value: "", required: true },
        module_function: { value: "", required: true },
        conditional: { value: "", required: true },
        value_to_check: { value: "", required: true },
        interval: { value: "", required: true },
        interval_unit: { value: "", required: true },
    },
    inputs: 0,
    outputs: 1,
    label() {
        return this.name ?? "Offline Event";
    },
    async onpaletteadd() {
        // update on Node-Add, so we can be speedy
        await updateModulesAutofill("#node-input-module_name");
    },
    async oneditprepare() {
        this.flow_name = this.flow_name ?? "What";

        // Update on edit in case the modules changed recently
        await updateModulesAutofill("#node-input-module_name");

        // Set the initial module functions
        const loadedModules = $("#node-input-module_name").val();

        if (typeof loadedModules === "string") {
            await updateModuleFunctionsAutofill("#node-input-module_function", loadedModules, true);
        } else {
            console.log("offline-event.html::oneditprepare Error! loadedModules !== string");
            console.log(`LoadedModules: ${JSON.stringify(loadedModules, null, 2)}`);
        }
        // --

        // When the user changes the module field, update the module functions
        $("#node-input-module_name").on("change", async () => {
            const newModule = $("#node-input-module_name").val();
            if (typeof newModule === "string") {
                await updateModuleFunctionsAutofill("#node-input-module_function", newModule);
            } else console.log("offline-event.html::oneditprepare ERROR!\nnewModule !== string");
        });

        $("#node-input-interval").typedInput({
            types: ["num"],
        });

        $("#node-input-interval_unit").typedInput({
            types: [
                {
                    value: "interval_unit",
                    options: [
                        "milliseconds",
                        "seconds",
                        "minutes",
                        "hours",
                    ],
                },
            ],
        });

        $("#node-input-conditional").typedInput({
            types: [
                {
                    value: "conditional",
                    options: [
                        ">",
                        "<",
                        ">=",
                        "<=",
                        "!=",
                        "==",
                    ],
                },
            ],
        });
        $("#node-input-value_to_check").typedInput({
            types: ["num", "bool"],
        });
    },
});
