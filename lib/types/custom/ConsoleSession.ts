/**
 * Represents an interactive console session to a container TTY.
 */
export interface ConsoleSession {
    /**
     * Register an event listener.
     * Only 'data' and 'close' is supported and one listener of each can be registered.
     */
    on(event: 'data' | 'close', listener: (chunk: Buffer) => void): this;

    /**
     * Write data to the console (e.g., commands).
     */
    write(data: string | Buffer): void;

    /**
     * Resize the console window.
     */
    resize(cols: number, rows: number): void;

    /**
     * Close the console session and release the TTY.
     */
    close(): void;
}