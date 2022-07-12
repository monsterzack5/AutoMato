export interface IPCOutput {
    module_function: string;
    module_name: string;
    return_output: boolean;
}

// Defined in runUserFunction due to:
// https://github.com/microsoft/TypeScript/issues/40344
// export enum OutputType {
//     number = 1,
//     float = 2,
//     bigint = 3,
//     bool = 4,
// }

export interface IPCInput {
    module_function?: string;
    module_name?: string;
    output?: string;
    output_type?: number;
}
