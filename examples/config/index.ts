import {Container} from "../../lib";

const name = "test-ct"

/**
 * In this example, a container is created and the calling process stdio is attached.
 */
async function main() {

    const c = new Container(name);
    c.setConfigItem("lxc.utsname", "test-host1");

    const rootfs  = c.getConfigItem("lxc.rootfs");
    console.log("Root FS: ", rootfs);

}

main().catch(console.error);