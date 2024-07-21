import {Container} from "../../lib";

const name = "test-ct"

/**
 * In this example, a container is created and the calling process stdio is attached.
 */
async function main() {

    const containers = await Promise.all([...Array(10).keys()].map(async (i) => {
            const c = new Container(name + i);
            if (!c.running) {
                await c.start();
                console.log(`Started ${c.name}`);
            } else {
                console.warn(`Container ${c.name} already running`);
            }
            return c;
        }
    ));

    console.table(containers.map((c) => {
        return {name: c.name, state: c.state}
    }));

}

main().catch(console.error);