import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    if(!c.defined){
        throw "Container not defined"
    }
    c.shutdown().then(()=> console.log("Container shutdown"))
}

main().catch(console.error)