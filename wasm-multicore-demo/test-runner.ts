// Run with: npx tsx test-runner.ts

import { chromium } from "playwright";
import { spawn, ChildProcess, execSync } from "child_process";
import { fileURLToPath } from "url";
import * as path from "path";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PORT = 3000;
const URL = `http://localhost:${PORT}`;
const TIMEOUT_MS = 30000;

async function main() {
    const projectDir = __dirname;
    let server: ChildProcess | null = null;
    let browser: Awaited<ReturnType<typeof chromium.launch>> | null = null;

    try {
        console.log("Building tests...");
        execSync("make test", { cwd: projectDir, stdio: "inherit" });

        console.log("Starting server...");
        server = spawn("bun", ["server.ts"], {
            cwd: projectDir,
            stdio: ["ignore", "pipe", "pipe"],
        });
        await new Promise((resolve) => setTimeout(resolve, 1000));

        console.log("Launching browser...");
        browser = await chromium.launch({ headless: false });
        const page = await browser.newPage();

        let result: "pass" | "fail" | "timeout" = "timeout";

        page.on("console", (msg) => {
            const text = msg.text();
            console.log(`[WASM] ${text}`);

            if (text.includes("[PASS]")) {
                result = "pass";
            } else if (text.includes("[FAIL]")) {
                result = "fail";
            }
        });

        await page.goto(URL);

        const startTime = Date.now();
        while (result === "timeout" && Date.now() - startTime < TIMEOUT_MS) {
            await new Promise((resolve) => setTimeout(resolve, 100));
        }

        console.log(`\nResult: ${result.toUpperCase()}`);
        process.exitCode = result === "pass" ? 0 : 1;

    } catch (err) {
        console.error("Error:", err);
        process.exitCode = 1;
    } finally {
        if (browser) await browser.close();
        if (server) server.kill();
    }
}

main();
