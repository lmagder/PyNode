declare module "@lmagder/pynode" {
  export type PyNodeWrappedPythonObject = {
    readonly __call__: (...args: PyNodeValue[]) => PyNodeValue;
    readonly __callasync__: (...args: [...PyNodeValue[], (error: string | null, result?: PyNodeValue) => void]) => void;
    readonly __callasync_promise__: (...args: PyNodeValue[]) => Promise<PyNodeValue>;
    readonly __getattr__: (field: string) => PyNodeValue;
    readonly __setattr__: (field: string, value: PyNodeValue) => void;
    readonly __repr__: (field: string) => string;
    readonly __pytype__: string;
  };
  export type PyNodeValue = null | number | string | boolean | PyNodeWrappedPythonObject | PyNodeValue[] | { [key: string]: PyNodeValue };

  export type PyNode = {
    readonly startInterpreter: (venvPython?: string) => void;
    readonly appendSysPath: (path: string) => void;
    readonly openFile: (filename: string) => PyNodeWrappedPythonObject;
    readonly import: (name: string) => PyNodeWrappedPythonObject;
    readonly eval: (expr: string) => number;
  };

  export const pynode: PyNode;

  export default pynode;
}
