import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if (!c.defined) {
        throw "Container not defined"
    }

    console.log("Shutting down the container...\n")
    await c.shutdown(30).then(
        () =>
            console.log("Container shutdown"),
        (reason) => {
            console.warn("Failed to shutdown container reason:", reason);
            return c.stop();
        })
}

main().catch(console.error)