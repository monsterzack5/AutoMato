import type { EditorNodeProperties, NodeDef } from "node-red";

/* General */

export interface OfflineEvent {
    flow_name: string;
    interval: number;
    interval_unit: string;

    module_name: string;
    module_function: string;

    conditional: string;
    value_to_check: number;
}

// TODO: Rename these and remove the Offline* part
export interface OfflineCommand {
    module_name: string;
    module_function: string;
}

export interface OfflineIf {
    conditional: string;
    value_to_check: number;

    module_name: string;
    module_function: string;
}

/* -------- */

/* CanRed JSON Endpoints */

// /modules
export type Modules = string[];

// /module/:module/functions
export type ModuleFunctions = string[];

/* -------- */

/* Offline Nodes */

export interface OfflineEventRegisterType extends EditorNodeProperties, OfflineEvent {}
export interface OfflineCommandRegisterType extends EditorNodeProperties, OfflineCommand {}
export interface OfflineIfRegisterType extends EditorNodeProperties, OfflineIf {}

export interface OfflineEventConfig extends NodeDef, OfflineEvent {}
export interface OfflineCommandConfig extends NodeDef, OfflineCommand {}
export interface OfflineIfConfig extends NodeDef, OfflineIf {}

/* -------- */

/* Online Nodes */

export interface OnlineCommandRegisterType extends EditorNodeProperties, OfflineCommand {}

export interface OnlineCommandConfig extends OfflineCommand, NodeDef {
//  This is defining an internal property that @types/node-red does
// not publicly expose.
    wires: [string[], string[]];
}

/* -------- */
