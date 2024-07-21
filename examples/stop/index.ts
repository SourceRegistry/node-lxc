import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if(!c.defined){
        throw "Container not defined"
    }
    c.stop().then(()=> console.log("Container stopped"))
}

main().catch(console.error)