const fs = require("fs");
const { argv } = require("process");

function byteToBinaryString(s) {
    return s.toString(2).padStart(8, "0");
}

const path = argv[2];

const bytes = fs.readFileSync(path);
console.log([...bytes].map(byteToBinaryString).join(" "));
