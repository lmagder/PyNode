declare module "@lmagder/pynode" {
  export type PyNodeWrappedPythonObject = {
    readonly call: (...args: PyNodeValue[]) => PyNodeValue;
    readonly get: (field: string) => PyNodeValue;
    readonly set: (field: string, value: PyNodeValue) => void;
    readonly repr: (field: string) => string;
    readonly pytype: string;
  };
  export type PyNodeValue = null | number | string | boolean | PyNodeWrappedPythonObject | PyNodeValue[] | { [key: string]: PyNodeValue };

  export type PyNode = {
    readonly startInterpreter: (path?: string) => void;
    readonly appendSysPath: (path: string) => void;
    readonly openFile: (filename: string) => void;
    readonly import: (name: string) => PyNodeWrappedPythonObject;
    readonly eval: (expr: string) => number;
    readonly call: (method: string, ...args: [...PyNodeValue[], (error: string | null, result: PyNodeValue) => void]) => void;
  };

  export const pynode: PyNode;

  export default pynode;
}
