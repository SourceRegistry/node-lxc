import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if (c.defined && c.running) {
        await c.addDeviceNode("/dev/network_latency");
        await c.removeDeviceNode("/dev/network_latency");
    }
}

main().catch(console.error)