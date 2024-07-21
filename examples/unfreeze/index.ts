import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if (!c.defined) {
        throw "Container not defined"
    }
    c.unfreeze().then(() => console.log("Container thawed"))
}

main().catch(console.error)