// Run with: npx tsx test-runner.ts
// (Bun has a known bug with Playwright on Windows)

import { chromium } from "playwright";
import { spawn, ChildProcess } from "child_process";

const PORT = 3000;
const URL = `http://localhost:${PORT}`;
const TIMEOUT_MS = 30000;

async function main() {
    let server: ChildProcess | null = null;
    let browser: Awaited<ReturnType<typeof chromium.launch>> | null = null;

    try {
        // Start the server
        console.log("Starting server...");
        server = spawn("bun", ["server.ts"], {
            cwd: import.meta.dirname,
            stdio: ["ignore", "pipe", "pipe"],
        });

        // Wait for server to be ready
        await new Promise((resolve) => setTimeout(resolve, 1000));

        // Launch browser (headless)
        console.log("Launching browser...");
        browser = await chromium.launch({ headless: true });
        const page = await browser.newPage();

        // Collect console output
        const logs: string[] = [];
        let testResult: "pass" | "fail" | "timeout" = "timeout";

        page.on("console", (msg) => {
            const text = msg.text();
            logs.push(text);
            console.log(`[WASM] ${text}`);

            if (text.includes("[PASS]")) {
                testResult = "pass";
            } else if (text.includes("[FAIL]")) {
                testResult = "fail";
            }
        });

        // Navigate to the page
        console.log(`Navigating to ${URL}...`);
        await page.goto(URL);

        // Wait for test to complete or timeout
        const startTime = Date.now();
        while (testResult === "timeout" && Date.now() - startTime < TIMEOUT_MS) {
            await new Promise((resolve) => setTimeout(resolve, 100));
        }

        // Print summary
        console.log("\n" + "=".repeat(50));
        if (testResult === "pass") {
            console.log("TEST PASSED");
            process.exitCode = 0;
        } else if (testResult === "fail") {
            console.log("TEST FAILED");
            process.exitCode = 1;
        } else {
            console.log("TEST TIMEOUT - no [PASS] or [FAIL] detected");
            process.exitCode = 1;
        }
        console.log("=".repeat(50));

    } finally {
        // Cleanup
        if (browser) {
            await browser.close();
        }
        if (server) {
            server.kill();
        }
    }
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});
