import {Container} from "../../lib";

const containerName = 'node-ct'; // Change to your container name

async function main() {
    const container = new Container(containerName);
    container.setConfigItem('lxc.log.file', `./${containerName}/container.log`)


    if (!container.defined || !container.running) {
        console.error(`Container '${containerName}' is not defined or not running.`);
        process.exit(1);
    }

    console.log(`📎 Attaching to console of container '${containerName}' (TTY 0)`);
    console.log(`💡 Type commands, press Ctrl+C or Ctrl+D to exit.\n`);

    try {
        // Open console
        const session = await container.consoleAsync(0);

        // Ensure we're in raw mode
        if (!process.stdin.isTTY) {
            console.error('Stdin is not a TTY');
            process.exit(1);
        }

        process.stdin.setRawMode(true);
        process.stdin.setEncoding('utf8');

        // Disable line buffering
        process.stdin.resume();

        // Handle data from container → stdout
        session.on('data', (chunk) => {
            process.stdout.write(chunk);
        });

        session.on('close', () => {
            process.stdout.write("Container closed session");
            process.exit(0);
        })

        // Handle input from user → container
        process.stdin.on('data', (data) => {
            if (data.toString() === '\x04') {
                // Ctrl+D
                session.close();
                process.exit(0);
            } else {
                session.write(data);
            }

        });

        // Handle terminal resize
        function handleResize() {
            const cols = process.stdout.columns || 80;
            const rows = process.stdout.rows || 24;
            try {
                session.resize(cols, rows);
            } catch (e) {
                // Ignore resize errors
            }
        }

        // Initial resize
        handleResize();

        // Watch for resize events
        process.stdout.on('resize', handleResize);

        // Optional: send SIGWINCH on SIGHUP, SIGTERM
        process.on('SIGINT', () => session.close());
        process.on('SIGTERM', () => session.close());

        // Cleanup on exit
        const cleanup = () => {
            session.close();
            process.stdin.setRawMode(false);
            process.exit(0);
        };

        process.on('exit', cleanup);
        process.on('SIGINT', cleanup);
        process.on('SIGTERM', cleanup);

        console.log('✅ Connected. You can now type inside the container.\n');
    } catch (err) {
        // @ts-ignore
        console.error('❌ Console error:', err?.message || err);
        process.exit(1);
    }
}

main().catch(err => {
    console.error('❌ Unexpected error:', err);
    process.exit(1);
});