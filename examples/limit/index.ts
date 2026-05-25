import {Container} from "../../lib";

async function main() {
    const c = new Container("node-ct");
    const limitInBytes = Number(c.getCGroupItem("memory.limit_in_bytes"));
    console.log(`current limitInBytes ${limitInBytes}`);
    const swapMemoryLimitInBytes = Number(c.getCGroupItem("memory.memsw.limit_in_bytes"));
    console.log(`current swapMemoryLimitInBytes ${swapMemoryLimitInBytes}`);

    c.setCGroupItem("memory.limit_in_bytes", (limitInBytes/4).toString())
    c.setCGroupItem("memory.memsw.limit_in_bytes", (swapMemoryLimitInBytes/4).toString())

}

main().catch(console.error)