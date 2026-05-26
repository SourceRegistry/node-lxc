/**
 * Result returned by {@link Container.execOutput}.
 */
export interface ExecOutputResult {
    /** Process exit code, or -1 if the process was killed by a signal. */
    exitCode: number;
    /** All data written to stdout, as a UTF-8 string. */
    stdout: string;
    /** All data written to stderr, as a UTF-8 string. */
    stderr: string;
}

/**
 * Streaming exec session returned by {@link Container.execAsync}.
 *
 * Register listeners before yielding control back to the event loop so no
 * output is missed.  The session closes automatically when the process exits.
 */
export interface ExecSession {
    /** Subscribe to a stdout chunk, stderr chunk, or process exit. */
    on(event: 'stdout', listener: (chunk: Buffer) => void): this;
    on(event: 'stderr', listener: (chunk: Buffer) => void): this;
    on(event: 'exit',   listener: (exitCode: number) => void): this;

    /**
     * Send a signal to the attached process.
     * @param signal - Signal number (default: SIGTERM = 15).
     */
    kill(signal?: number): void;

    /** `true` once the process has exited and all resources have been freed. */
    get closed(): boolean;
}
