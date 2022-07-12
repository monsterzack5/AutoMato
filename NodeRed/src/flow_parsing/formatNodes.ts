import {
    OfflineEventNode, AutomatoMainEvent, OfflineCommandNode,
    AutomatoSecondaryEvent, OfflineIfNode, AutomatoIfEvent,
} from "../@types/flow_parsing_types";

export function getFormattedEvent(node: OfflineEventNode): AutomatoMainEvent {
    return {
        flow_name: node.flow_name,
        module_function: node.module_function,
        module_name: node.module_name,
        conditional: node.conditional,
        value_to_check: node.value_to_check,
        interval: node.interval,
        interval_unit: node.interval_unit,
        section_number: 0,
    };
}

export function getFormattedCommand(node: OfflineCommandNode, sectionNumber: number, nextSection: number): AutomatoSecondaryEvent {
    return {
        module_function: node.module_function,
        module_name: node.module_name,
        section_number: sectionNumber,
        next_section: nextSection,
    };
}

export function getFormattedIf(node: OfflineIfNode, sectionNumber: number, ifTrue: number, ifFalse: number): AutomatoIfEvent {
    return {
        module_name: node.module_name,
        module_function: node.module_function,
        conditional: node.conditional,
        value_to_check: node.value_to_check,
        section_number: sectionNumber,
        if_true: ifTrue,
        if_false: ifFalse,
    };
}
