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
    addonPath = path.join(__dirname, '..','build', mode, `${base_name}.node`);
} else {
    const archToTriplet: Record<string, string> = {
        x64:   'x86_64-linux-gnu',
        arm64: 'aarch64-linux-gnu',
    };
    const triplet = platform === 'linux' ? archToTriplet[arch] : undefined;
    if (!triplet) {
        throw new Error(`Unsupported platform or architecture: ${platform}-${arch}`);
    }
    addonPath = path.join(__dirname, '..', 'bin', triplet, `${base_name}.node`);
}


export default require(addonPath)
























