import {Container, LXC, LXC_LOGLEVEL} from "../lib";

import {describe, it} from "node:test";
import assert = require("node:assert");

const name = 'node-ct';

describe("Container", () => {

    console.info(`LXC version(${LXC.GetVersion()})`);
    const c = new Container(name);

    describe("#Setup Logging", () =>
        it("should setup 'VERBOSE' logging for the container", () => {
            c.setConfigItem("lxc.log.file", `./${name}/.log`);
            c.setConfigItem("lxc.log.level", LXC_LOGLEVEL.TRACE);
        })
    );

    describe("#Create", () =>
        it("should define a container called `node-ct`", async () => {
            if (!c.defined) {
                await c.create({
                    template: "download",
                    argv: ["--dist", "ubuntu", "--release", "lunar", "--arch", "amd64"]
                })
            }
            assert.notEqual(c.defined, false);
        })
    );

    describe("#Name", async () =>
        it("should return the containers name `node-ct`", async () => {
            assert.equal(c.name, "node-ct");
        })
    );

    describe("#Setup Networking", async () =>
        it("should add a networking capabilities to the containers", async () => {
            /* Setup network connection */
            c.setConfigItem("lxc.net.0.type", "veth");
            c.setConfigItem("lxc.net.0.link", "lxcbr0");
            c.setConfigItem("lxc.net.0.flags", "up");
            c.setConfigItem("lxc.net.0.hwaddr", "00:16:3e:xx:xx:xx");
        })
    );

    describe("#Start", async () =>
        it("should add a networking capabilities to the containers", async () => {
            // Setup network connection
            assert(c.defined, "Container not defined");
            await c.start();
            assert.equal(c.running, true);
        })
    );

    describe("#State", async () =>
        it("should return the state of the container == 'RUNNING'", async () => {
            // Setup network connection
            assert(c.defined, "Container not defined");
            assert.equal(c.state, "RUNNING");
        })
    );

    describe("#Reboot", async () =>
        it("should reboot the container", async () => {
            await c.reboot();
            assert.equal(c.state, "RUNNING");
        })
    );

    describe("#Shutdown", async () =>
        it("should return the state of the container == 'STOPPED'", async () => {
            await c.shutdown();
            assert.equal(c.state, "STOPPED");
        })
    );

    describe("#Stop", async () =>
        it("should return the state of the container == 'STOPPED'", async () => {
            await c.stop();
            assert.equal(c.state, "STOPPED");
        })
    );
})
