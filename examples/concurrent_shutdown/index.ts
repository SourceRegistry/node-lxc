import {Container} from "../../lib";

const name = "test-ct"

/**
 * In this example, a container is created and the calling process stdio is attached.
 */
async function main() {

    const containers = await Promise.all([...Array(10).keys()].map(async (i) => {
            let c = new Container(name + i);
            if (c.defined) {
                console.log(`Shutting down ${c.name}`);
                await c.shutdown();
            } else {
                console.warn(`Container ${c.name} not defined`);
            }
            return c;
        }
    ));

    console.table(containers.map((c) => {
        return {name: c.name, state: c.state}
    }));

}

main().catch(console.error);