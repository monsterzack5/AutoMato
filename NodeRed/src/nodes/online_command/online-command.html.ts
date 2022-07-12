import type { EditorRED } from "node-red";
import type { OnlineCommandRegisterType } from "../../@types/node_types";

import { updateModuleFunctionsAutofill, updateModulesAutofill } from "../common_browser/autofill";

declare const RED: EditorRED;

RED.nodes.registerType<OnlineCommandRegisterType>("online-command", {
    category: "Automato",
    color: "#87A980",
    // TODO:
    // Make sure these are 1 to 1 with all node-input-* css IDs
    // module-function should be converted to functions ?
    defaults: {
        module_name: { value: "", required: true },
        module_function: { value: "", required: true },
    },
    inputs: 1,
    outputs: 1,
    // icon: "file.png",
    label() {
        // TODO: Make the label the current moduleFunction
        // console.log("Running label() on online-command");
        // const currentFunction = $("#node-input-module_function").val()?.toString();
        // if (typeof currentFunction === "string") {
        // console.log(`currentFunction: ${currentFunction}`);
        // return "currentFunction";
        // }
        // console.log(`typeof currentFunction: ${typeof currentFunction}, currentFunction: ${currentFunction}`);

        // return this.name;
        return this.name ?? "online Command";
    },
    async onpaletteadd() {
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
            console.log("online-command.html::oneditprepare Error! loadedModules !== string");
            console.log(`LoadedModules: ${JSON.stringify(loadedModules, null, 2)}`);
        }

        // When the user changes the module field, update the module functions
        $("#node-input-module_name").on("change", async () => {
            const newModule = $("#node-input-module_name").val();
            if (typeof newModule === "string") {
                await updateModuleFunctionsAutofill("#node-input-module_function", newModule);
            } else console.log("online-command.html::oneditprepare ERROR!\nnewModule !== string");
        });
    },
});
