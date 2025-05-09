import {Images} from "../../lib";

async function main() {
    const available = await Images.Available()
    console.log(available)
    const list = await Images.List()
    console.log(list)
}

main().catch(console.error)