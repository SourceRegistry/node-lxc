import {Container, LXC_LOGLEVEL} from "../../lib";
import {readdir} from "node:fs/promises";

const name = "test-ct"
const checkpointDir = `/tmp/${name}`

/**
 * In this example, a container is created and the calling process stdio is attached.
 */
async function main() {
    const c = new Container(name);

    c.setConfigItem("lxc.log.file", `./${name}/.log`);
    c.setConfigItem("lxc.log.level", LXC_LOGLEVEL.TRACE);

    if (!c.defined) {
        console.log("Container creating...");
        await c.create({
            template: "download",
            argv: ["--dist", "ubuntu", "--release", "lunar", "--arch", "amd64"]
        });
        console.log("Created");
    }
    if (!c.running) {
        await c.start();
    }
    console.log(`Creating lcone for container '${name}' in directory '${checkpointDir}'`);
    await c.checkpoint(checkpointDir, true, true); //TODO

    console.table(await readdir(checkpointDir));

}

main().catch(console.error);