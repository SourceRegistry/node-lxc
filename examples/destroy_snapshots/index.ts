import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if (c.defined && !c.running) {
        await c.snapshotDestroy(true);
    }
}

main().catch(console.error)