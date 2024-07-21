import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if (c.defined) {
        console.table(Promise.all((await c.getInterfaces()).map(async (inf) => {
            return {interface: inf, ipv4: await c.getIPs(inf, "inet"), ipv6: await c.getIPs(inf, "inet6", 0)}
        })));
    }
}

main().catch(console.error)