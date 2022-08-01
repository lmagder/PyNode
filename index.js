import { createRequire } from "node:module";

const require = createRequire(import.meta.url);

export const pynode = require("./build/Release/PyNode");
