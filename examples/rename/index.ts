import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if(!c.defined){
        throw "Container not defined"
    }
    c.name = "new-name";
}

main().catch(console.error)