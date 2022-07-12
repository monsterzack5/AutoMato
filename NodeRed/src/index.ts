import http from "http";
import express from "express";
import RED from "node-red";
import moduleRouter from "./ModuleRouter/moduleRouter";
import { checkENVVars } from "./ModuleRouter/utils/checkENVVars";

// Make sure ENV Vars are properly set
checkENVVars();

// Create an Express App, and a Server
const app = express();
const server = http.createServer(app);

/* Configure Node-Red */
const settings = {
    httpAdminRoot: "/",
    httpNodeRoot: "/",
    userDir: process.env.userDir,
    functionGlobalContext: { },
    uiPort: 1880,
    uiHost: "localhost",
    flowFile: "flows.json",
    credentialSecret: "TODO: Change me",
};

RED.init(server, settings);

// Server Landing page, public, and HTTP Node endpoint
app.use(settings.httpAdminRoot, RED.httpAdmin);
app.use(settings.httpNodeRoot, RED.httpNode);
app.use("/", express.static("public"));

RED.start();

// ---

// Add our own moduleRouter
app.use("/automato/", moduleRouter);

// 404
app.all("*", (_, res) => res.status(404).json({ error: "Page Not Found" }));

// Start Express
server.listen(1880);
