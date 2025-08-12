import {Container} from "../../../lib";
import {Console} from "../../../lib/types/custom"; // Your native binding container

async function main() {
    const container = new Container("node-ct");
    container.setConfigItem('lxc.log.file', './node-ct/container.log');

    const consoleInstance = new Console(container);

    consoleInstance.on("open", () => console.log("Console opened"));
    consoleInstance.on("data", (data) => process.stdout.write(data));
    consoleInstance.on("error", (err) => console.error("Error:", err));
    consoleInstance.on("close", () => console.log("Console closed"));

    process.stdin.on('data', (chunk) => consoleInstance.write(chunk))

    try {
        await consoleInstance.open();
        console.log("Console session finished");
    } catch (err) {
        console.error("Failed to open console:", err);
    }
}

main();
