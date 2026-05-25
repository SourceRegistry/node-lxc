import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if(!c.defined){
        throw "Container not defined"
    }
    c.start().then(()=> console.log("Container started"))
}

main().catch(console.error)