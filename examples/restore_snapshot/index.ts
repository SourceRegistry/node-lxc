import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if (!c.defined) {
        throw "Container not defined"
    }
    await c.snapshotRestore("snap0", "node-ct-restored");
}

main().catch(console.error)