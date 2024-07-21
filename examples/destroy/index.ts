import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if (c.defined && !c.running) {
        await c.destroy();
    }
}

main().catch(console.error)