import type { EditorRED } from "node-red";
import type { OfflineIfRegisterType } from "../../@types/node_types";

import { updateModuleFunctionsAutofill, updateModulesAutofill } from "../common_browser/autofill";

declare const RED: EditorRED;

RED.nodes.registerType<OfflineIfRegisterType>("offline-if", {
    category: "Automato",
    color: "#a6bbcf",
    // TODO:
    // Make sure these are 1 to 1 with all node-input-* css IDs
    // module-function should be converted to functions ?
    defaults: {
        module_name: { value: "" },
        module_function: { value: "" },
        conditional: { value: "", required: true },
        value_to_check: { value: "", required: true },
    },
    inputs: 1,
    outputs: 2,
    // icon: "file.png",
    label() {
        return this.name ?? "Offline If";
    },
    async onpaletteadd() {
        // FIXME, conditionals should also be true or false
        // update on Node-Add, so we can be speedy
        await updateModulesAutofill("#node-input-module_name");
    },
    async oneditprepare() {
        // Update on edit in case the modules changed recently
        await updateModulesAutofill("#node-input-module_name");

        // Set the initial module functions
        const loadedModules = $("#node-input-module_name").val();

        if (typeof loadedModules === "string") {
            await updateModuleFunctionsAutofill("#node-input-module_function", loadedModules, true);
        } else {
            console.log("offline-if.html::oneditprepare Error! loadedModules !== string");
            console.log(`LoadedModules: ${JSON.stringify(loadedModules, null, 2)}`);
        }

        // When the user changes the module field, update the module functions
        $("#node-input-module_name").on("change", async () => {
            const newModule = $("#node-input-module_name").val();
            if (typeof newModule === "string") {
                await updateModuleFunctionsAutofill("#node-input-module_function", newModule);
            } else console.log("offline-if.html::oneditprepare ERROR!\nnewModule !== string");
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
