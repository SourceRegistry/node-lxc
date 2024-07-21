import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    c.getKeys().forEach((key) => {
        console.log(key, "=", c.getConfigItem(key)?.trim())
    })
}

main().catch(console.error)