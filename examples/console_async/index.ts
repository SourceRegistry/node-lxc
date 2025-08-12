import {Container} from "../../lib";
import {closeSync} from "node:fs";

const name = "node-ct"

/**
 * In this example, a container is created and the calling process stdio is attached.
 */
async function main() {
    const c = new Container(name);
    c.setConfigItem('lxc.log.file', `./${name}/container.log`)

    if (c.defined && c.running) {
        let session = await c.consoleAsync(0)
        session.on('data', (chunk) => {
            process.stdout.write(chunk)
        })



        session.write("ls -la\r\n"); // send command
        session.write("exit\r\n");

        await new Promise<void>((resolve) => setTimeout(() => resolve(), 5000));
        session.close();


        session = await c.consoleAsync(0)
        session.on('data', (chunk) => {
            process.stdout.write(chunk)
        })



        session.write("ls -la\r\n"); // send command
        session.write("exit\r\n");

        await new Promise<void>((resolve) => setTimeout(() => resolve(), 5000));
        session.close();


    } else {
        console.warn(`Container '${name}' not defined or not running`);
    }
}

main().catch(console.error);