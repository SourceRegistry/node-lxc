import {Container} from "../../lib";

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
    console.log(`Attaching to container '${name}'`)
    await c.attach({
        stdio: [process.stdin.fd, process.stdout.fd, process.stderr.fd],
    });
    console.warn(`Destroying '${name}'`)
    await c.destroy({force: true});
}

main().catch(console.error);