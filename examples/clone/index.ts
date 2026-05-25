import {Container, LXC_LOGLEVEL} from "../../lib";

const name = "test-ct"

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
    if (c.running) {
        console.warn("Container needs to be stopped to create a clone")
        console.log("Container shutting down...");
        await c.shutdown();
        console.log("Done");

    }
    console.log("Creating clone...")
    await c.clone({newname: name + "2"});
    console.log("Done");
}

main().catch(console.error);