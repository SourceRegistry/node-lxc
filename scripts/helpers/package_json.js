#!/usr/bin/env node

const path = require("path")
const package = require(path.join(__dirname, "..", "..", "package.json"))

const jsonPath = process.argv[2].split("/");


let temp = package;
for (let string of jsonPath) {
    temp = temp[string]
    if (!temp) {
        console.error("Unable to find item");
        process.exit(1);
    }
}
console.log(typeof temp === 'object' ? JSON.stringify(temp) : temp);