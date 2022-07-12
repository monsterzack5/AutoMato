import { Router } from "express";
import { updateFlowsIfNeeded } from "../flow_parsing/updateFlowsIfNeeded";
import { db } from "./utils/db";

const moduleRouter = Router();

const getAllModules = db.prepare("SELECT name FROM can_modules");
const getModuleFunctions = db.prepare(
    "SELECT name FROM can_module_commands WHERE module_uid = (SELECT uid FROM can_modules WHERE name = ?)",
);

// TODO: make a type for this.
const flattenSqliteOutput = (output: Array<{ name: string}>) => {
    const reduced = output.reduce((acc: string[], key) => {
        acc.push(key.name);
        return acc;
    }, []);
    return reduced;
};

// Returns: ["Module1", "Module2"...]
moduleRouter.get("/modules", (_, res) => {
    const modules = flattenSqliteOutput(getAllModules.all());
    res.json(modules);
});

// Returns: ["Function1", "Function2"...]
moduleRouter.get("/module/:module/functions", (req, res) => {
    const moduleFunctions = flattenSqliteOutput(getModuleFunctions.all(req.params.module));
    res.json(moduleFunctions);
});

moduleRouter.get("/updateflows", (_, res) => {
    res.sendStatus(200);
    updateFlowsIfNeeded();
});

export default moduleRouter;
