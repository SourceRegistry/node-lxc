import {LXC} from "../../lib";

async function main() {
    console.log("===== ACTIVE =====");
    LXC.ListAllActiveContainers().forEach(console.log)
    console.log("===== DEFINED =====");
    LXC.ListAllDefinedContainers().forEach(console.log)
    console.log("===== ALL =====");
    LXC.ListAllContainers().forEach(console.log)
}

main().catch(console.error)