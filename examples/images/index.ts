import {Images} from "../../lib";

async function main() {
    const list = await Images.List()
    console.log(list)
}

main().catch(console.error)