/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

import path from 'path';

const platform = process.platform;
const arch = process.arch;

const base_name = "node-lxc"
const is_package = __dirname.includes("node_modules");
let addonPath;
if (!is_package) {
    const mode = process.env["NODE_ENV"] === "development" ? "Debug" : "Release";
    if (mode === "Debug") {
        console.warn("!!!RUNNING IN DEVELOPMENT MODE!!!");
    }
    addonPath = path.join(__dirname, '..', 'build', mode, `${base_name}.node`);
} else {
    if (platform === 'linux' && arch === 'x64') {
        addonPath = path.join(__dirname, '..', 'bin', 'x86_64-linux-gnu', `${base_name}.node`);
    } else {
        throw new Error(`Unsupported platform or architecture: ${platform}-${arch}`);
    }
}


export default require(addonPath)
























