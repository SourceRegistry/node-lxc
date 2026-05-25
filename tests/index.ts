import { describe, it, before, after } from "node:test";
import assert from "node:assert/strict";
import {
    Container, LXC,
    GetVersion, GetGlobalConfigItem,
    ListAllContainers, ListAllDefinedContainers, ListAllActiveContainers,
    ConfigItemIsSupported, HasApiExtension, GetWaitStates,
    ContainerState, ContainerStats,
} from "../lib";

const isRoot = process.getuid?.() === 0;
const TEST_CONTAINER = "node-lxc-test";

// ─── Global API ───────────────────────────────────────────────────────────────

describe("Global API", () => {
    it("GetVersion() returns a semver string", () => {
        const v = GetVersion();
        assert.match(v, /^\d+\.\d+\.\d+/, `Expected semver, got: ${v}`);
    });

    it("GetGlobalConfigItem('lxc.lxcpath') returns an absolute path", () => {
        const p = GetGlobalConfigItem("lxc.lxcpath");
        assert.ok(typeof p === "string" && p.startsWith("/"), `Expected absolute path, got: ${p}`);
    });

    it("ListAllContainers() returns string[] for given lxcpath", () => {
        // Pass an explicit path to avoid requiring root access to the default lxcpath
        const list = ListAllContainers("/tmp");
        assert.ok(Array.isArray(list));
        for (const n of list) assert.equal(typeof n, "string");
    });

    it("ListAllDefinedContainers() returns string[] for given lxcpath", () => {
        const list = ListAllDefinedContainers("/tmp");
        assert.ok(Array.isArray(list));
        for (const n of list) assert.equal(typeof n, "string");
    });

    it("ListAllActiveContainers() returns string[]", () => {
        const list = ListAllActiveContainers();
        assert.ok(Array.isArray(list));
        for (const n of list) assert.equal(typeof n, "string");
    });

    it("ConfigItemIsSupported() returns true for known key, false for unknown", () => {
        assert.equal(ConfigItemIsSupported("lxc.log.level"), true);
        assert.equal(ConfigItemIsSupported("lxc.nonexistent.key.xyz"), false);
    });

    it("HasApiExtension() returns a boolean", () => {
        assert.equal(typeof HasApiExtension("container_snapshot_comments"), "boolean");
    });

    it("GetWaitStates() includes all ContainerState values", () => {
        const states = GetWaitStates();
        assert.ok(Array.isArray(states) && states.length >= 8, "Expected ≥8 states");
        const expected: ContainerState[] = [
            "STOPPED", "STARTING", "RUNNING", "STOPPING",
            "ABORTING", "FREEZING", "FROZEN", "THAWED",
        ];
        for (const s of expected) {
            assert.ok(states.includes(s), `Missing state: ${s}`);
        }
    });
});

// ─── Container (offline — no running container required) ──────────────────────

describe("Container (offline)", () => {
    let c: Container;

    before(() => {
        c = new Container(TEST_CONTAINER);
    });

    it("name getter returns the name passed to the constructor", () => {
        assert.equal(c.name, TEST_CONTAINER);
    });

    it("defined returns false for a non-existent container", () => {
        assert.equal(c.defined, false);
    });

    it("state returns 'STOPPED' for an undefined container", () => {
        assert.equal(c.state, "STOPPED");
    });

    it("running returns false for an undefined container", () => {
        assert.equal(c.running, false);
    });

    it("initPID returns -1 when not running", () => {
        assert.equal(c.initPID, -1);
    });

    it("mayControl() returns true for our own container handle", () => {
        assert.equal(c.mayControl(), true);
    });

    it("getKeys() returns a non-empty string array", () => {
        const keys = c.getKeys();
        assert.ok(Array.isArray(keys) && keys.length > 0, "Expected non-empty key array");
        for (const k of keys) assert.equal(typeof k, "string");
    });

    it("getKeys(prefix) returns only keys matching the prefix", () => {
        const allKeys = c.getKeys();
        const logKeys = c.getKeys("lxc.log");
        for (const k of logKeys) {
            assert.ok(k.startsWith("lxc.log"), `Key '${k}' does not match prefix 'lxc.log'`);
        }
        assert.ok(logKeys.length <= allKeys.length);
    });

    it("getConfigItems() returns Record<string, string|null>", () => {
        const items = c.getConfigItems();
        assert.ok(typeof items === "object" && items !== null);
        assert.ok(Object.keys(items).length > 0, "Expected non-empty config items");
        for (const [k, v] of Object.entries(items)) {
            assert.equal(typeof k, "string");
            assert.ok(v === null || typeof v === "string", `Expected string|null for key ${k}, got ${typeof v}`);
        }
    });

    it("getConfigItems(prefix) returns only matching keys", () => {
        const filtered = c.getConfigItems("lxc.log");
        for (const k of Object.keys(filtered)) {
            assert.ok(k.startsWith("lxc.log"), `Key '${k}' does not match prefix 'lxc.log'`);
        }
    });

    it("getConfigItems() and getKeys() return the same key count", () => {
        const keys = c.getKeys();
        const items = c.getConfigItems();
        assert.equal(Object.keys(items).length, keys.length);
    });

    it("setConfigItem / getConfigItem round-trips", () => {
        c.setConfigItem("lxc.log.file", "/tmp/node-lxc-test.log");
        assert.equal(c.getConfigItem("lxc.log.file"), "/tmp/node-lxc-test.log");
    });

    it("clearConfigItem removes the key value", () => {
        c.setConfigItem("lxc.log.file", "/tmp/node-lxc-test.log");
        c.clearConfigItem("lxc.log.file");
        assert.equal(c.getConfigItem("lxc.log.file"), null);
    });

    it("clearConfig() resets all in-memory config", () => {
        c.setConfigItem("lxc.log.file", "/tmp/node-lxc-test.log");
        c.clearConfig();
        assert.equal(c.getConfigItem("lxc.log.file"), null);
    });

    it("configPath getter returns an absolute path", () => {
        const p = c.configPath;
        assert.ok(typeof p === "string" && p.startsWith("/"), `Expected absolute path, got: ${p}`);
    });

    it("getRunningConfigItem() returns null when container is not running", () => {
        assert.equal(c.getRunningConfigItem("lxc.log.level"), null);
    });

    it("getCGroupItem() returns undefined when container is not running", () => {
        assert.equal(c.getCGroupItem("memory.usage_in_bytes"), undefined);
    });

    it("setTimeout() throws or returns boolean depending on LXC build", () => {
        try {
            const result = c.setTimeout(-1);
            assert.equal(typeof result, "boolean", "Expected boolean return from setTimeout");
        } catch (e: any) {
            assert.ok(
                e instanceof TypeError && e.message.includes("set_timeout"),
                `Unexpected error: ${e.message}`
            );
        }
    });

    it("error getter returns { num, string } object (string may be null)", () => {
        const err = c.error;
        assert.ok(typeof err === "object" && err !== null);
        assert.equal(typeof err.num, "number");
        assert.ok(err.string === null || typeof err.string === "string");
    });
});

// ─── Integration lifecycle (requires root / privileged LXC access) ────────────

describe("Container lifecycle (integration)", { skip: isRoot ? false : "requires root" }, () => {
    let c: Container;

    before(() => {
        c = new Container(TEST_CONTAINER);
        c.setConfigItem("lxc.log.file", `./${TEST_CONTAINER}.log`);
        c.setConfigItem("lxc.log.level", "WARN");
    });

    after(async () => {
        try { if (c.running) await c.stop(); } catch { /* ignore */ }
        try { if (c.defined) await c.destroy({ include_snapshots: true }); } catch { /* ignore */ }
    });

    it("create() defines the container", async () => {
        if (!c.defined) {
            await c.create({
                template: "download",
                argv: ["--dist", "ubuntu", "--release", "jammy", "--arch", "amd64"],
            });
        }
        assert.equal(c.defined, true);
    });

    it("start() starts the container and state becomes RUNNING", async () => {
        c.setConfigItem("lxc.net.0.type", "veth");
        c.setConfigItem("lxc.net.0.link", "lxcbr0");
        c.setConfigItem("lxc.net.0.flags", "up");
        await c.start();
        assert.equal(c.running, true);
        assert.equal(c.state, "RUNNING");
    });

    it("initPID returns a positive integer when running", () => {
        assert.ok(c.initPID > 0, `Expected positive PID, got: ${c.initPID}`);
    });

    it("stats() returns an object with cgroup metric strings or null", async () => {
        const stats: ContainerStats = await c.stats();
        assert.ok(typeof stats === "object" && stats !== null);
        const keys: (keyof ContainerStats)[] = [
            "memory.usage_in_bytes", "memory.limit_in_bytes", "memory.memsw.usage_in_bytes",
            "cpuacct.usage", "cpu.stat", "blkio.throttle.io_service_bytes",
        ];
        for (const k of keys) {
            assert.ok(k in stats, `Missing key: ${k}`);
            assert.ok(stats[k] === null || typeof stats[k] === "string");
        }
    });

    it("exec() returns exit code 0 for /bin/true", async () => {
        const code = await c.exec({ argv: ["/bin/true"] });
        assert.equal(code, 0);
    });

    it("exec() returns non-zero exit code for /bin/false", async () => {
        const code = await c.exec({ argv: ["/bin/false"] });
        assert.notEqual(code, 0);
    });

    it("exec() runs a command and captures exit code correctly", async () => {
        const code = await c.exec({ argv: ["/bin/sh", "-c", "exit 42"] });
        assert.equal(code, 42);
    });

    it("reboot() completes and container returns to RUNNING", async () => {
        const ok = await c.reboot(30);
        assert.equal(ok, true);
        assert.equal(c.running, true);
        assert.equal(c.state, "RUNNING");
    });

    it("shutdown() stops the container gracefully", async () => {
        const ok = await c.shutdown(30);
        assert.equal(ok, true);
        assert.equal(c.running, false);
        assert.equal(c.state, "STOPPED");
    });

    it("destroy() removes the container definition", async () => {
        await c.destroy();
        assert.equal(c.defined, false);
    });
});
