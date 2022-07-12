export async function updateModuleFunctionsAutofill(htmlElementID: string, moduleName: string, firstRun = false): Promise<void> {
    if (moduleName.length === 0) {
        console.log("offline-event.html::updateModuleFUnctionsAutofill ERROR!: moduleName.length === 0");
    }
    let output: string[] = [];
    try {
        const data = await fetch(`http://localhost:1880/automato/module/${moduleName}/functions`);
        output = await data.json();
    } catch (e) {
        console.log(`offline-event.html::updateModuleFunctionsAutofill ERROR!\n${e}`);
        output.push("Error");
    }

    if (firstRun) {
        // For the first run, we need to call:
        // .typedInput(x) -> (x: WidgetTypedInputTypeDefinition)
        $(htmlElementID).typedInput({
            types: [
                {
                    value: "module_function",
                    options: output,
                },
            ],
        });
        return;
    }

    // for the second run, we need to call:
    // .typedInput(x, y) -> (x: "types", y: Array<WidgetTypedInputTypeDefinition>)
    $(htmlElementID).typedInput("types", [
        {
            value: "module_function",
            options: output,
        },
    ]);
}

export async function updateModulesAutofill(htmlElementID: string): Promise<void> {
    let output: string[] = [];
    try {
        const data = await fetch("http://localhost:1880/automato/modules");
        const parsedData = await data.json();
        output = parsedData;
    } catch (e) {
        console.log(`offline-event.html ERROR!\n${e}`);
        console.log(`offline-event.html::updateModulesAutofill ERROR!\n${e}`);
        output.push("Error");
    }

    $(htmlElementID).typedInput({
        types: [
            {
                value: "module",
                options: output,
            },
        ],
    });
}
