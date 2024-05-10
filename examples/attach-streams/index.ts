import {Container} from "../../lib/bindings";
import {openSync} from "node:fs";

const name = "test-ct"

/**
 * In this example, a container is created and the calling process stdio is attached.
 */
async function main() {
    const c = new Container(name);
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
    console.log(`Attaching to container '${name}'`);

    const stdoutFd = openSync("examples/attach-streams/stdout", "w+"); //TODO: Check if this works
    const stderrFd = openSync("examples/attach-streams/stderr", "w+"); //TODO: Check if this works


    await c.attach({stdio: [process.stdin.fd, stdoutFd, stderrFd]});  //TODO: Check if this works

    console.warn(`Destroying '${name}'`)
    await c.destroy({force: true});
}

main().catch(console.error);