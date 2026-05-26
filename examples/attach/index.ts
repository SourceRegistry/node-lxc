import {execSync} from "child_process";
import {Container} from "../../lib";

const name = "test-ct"

/**
 * In this example, a container is created and the calling process stdio is attached.
 */
async function main() {
    // Start lxc-net so dnsmasq provides DHCP on lxcbr0 (10.0.3.0/24)
    // execSync("service lxc-net start", {stdio: "ignore"});

    const c = new Container(name);
    if (!c.defined) {
        console.log("Container creating...");
        await c.create({
            template: "download",
            argv: ["--dist", "ubuntu", "--release", "noble", "--arch", "amd64"]
        });
        console.log("Created");
    }
    if (!c.running) {
        await c.start();
    }
    console.log(`Attaching to container '${name}'`)
    await c.attach({
        stdio: [process.stdin.fd, process.stdout.fd, process.stderr.fd],
        extra_env_vars: [`TERM=${process.env.TERM ?? "xterm"}`],
    });
    console.warn(`Destroying '${name}'`)
    await c.destroy({force: true});
}

main().catch(console.error);