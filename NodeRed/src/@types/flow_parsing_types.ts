import type { OfflineEvent, OfflineCommand, OfflineIf } from "./node_types";

export interface AutomatoMainEvent {
    section_number: 0;
    flow_name: string;
    module_function: string;
    module_name: string;
    conditional: string;
    value_to_check: number;
    interval: number;
    interval_unit: string;
}

export interface AutomatoSecondaryEvent {
    section_number: number;
    next_section: number;
    module_function: string;
    module_name: string;
}

export interface AutomatoIfEvent {
    section_number: number;
    module_name: string;
    module_function: string;
    conditional: string;
    value_to_check: number;
    if_true: number;
    if_false: number;
}

export interface NodeRedNode {
    id: string;
    type: string;
    z: string;
    x: number;
    y: number;
    wires: [string[], string[]];
}

export interface OfflineEventNode extends OfflineEvent, NodeRedNode {
    type: "offline-event";
    d: false | undefined;
}

export interface OfflineCommandNode extends OfflineCommand, NodeRedNode {
    type: "offline-command";
    d: false | undefined;
}

export interface OfflineIfNode extends OfflineIf, NodeRedNode {
    type: "offline-if";
    d: false | undefined;
}

export type FilteredFlowsFile = Array<OfflineEventNode | OfflineCommandNode | OfflineIfNode>;

export type AnyChildNode = OfflineCommandNode | OfflineIfNode | undefined;

export type EventFlow = [AutomatoMainEvent, ...Array<AutomatoSecondaryEvent | AutomatoIfEvent>];
