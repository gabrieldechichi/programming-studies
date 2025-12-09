// Run with: npx tsx test-runner.ts
// (Bun has a known bug with Playwright on Windows)

import { chromium } from "playwright";
import { spawn, ChildProcess, execSync } from "child_process";
import * as fs from "fs";
import * as path from "path";
import { fileURLToPath } from "url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PORT = 3000;
const URL = `http://localhost:${PORT}`;
const TIMEOUT_MS = 30000;

const DEMOS = [
    // "demo_thread_create.c",
    // "demo_tls.c",
    // "demo_shared_memory.c",
    // "demo_mutex.c",
    // "demo_barrier.c",
    // "demo_condvar.c",
    // "demo_atomics.c",
    // "demo_semaphore.c",
    "demo_rwlock.c",
    // "demo_detach.c",
    // "demo_thread_attr.c",
    // "demo_once.c",
];

function setActiveDemo(demoFile: string, projectDir: string) {
    const mainCPath = path.join(projectDir, "main.c");
    let content = fs.readFileSync(mainCPath, "utf-8");

    // Comment out all demos
    for (const demo of DEMOS) {
        const includePattern = new RegExp(`^(\\s*)#include "demos/${demo}"`, "m");
        const commentedPattern = new RegExp(`^(\\s*)// #include "demos/${demo}"`, "m");

        if (includePattern.test(content)) {
            content = content.replace(includePattern, `$1// #include "demos/${demo}"`);
        }
    }

    // Uncomment the target demo
    const targetCommented = new RegExp(`^(\\s*)// #include "demos/${demoFile}"`, "m");
    content = content.replace(targetCommented, `$1#include "demos/${demoFile}"`);

    fs.writeFileSync(mainCPath, content);
}

function buildProject(projectDir: string): boolean {
    try {
        execSync("make clean && make", {
            cwd: projectDir,
            stdio: "pipe",
        });
        return true;
    } catch (err) {
        console.error("Build failed:", (err as any).stderr?.toString());
        return false;
    }
}

async function runTest(
    page: Awaited<ReturnType<typeof chromium.launch>>["newPage"] extends () => Promise<infer P> ? P : never,
    demoName: string
): Promise<"pass" | "fail" | "timeout"> {
    const logs: string[] = [];
    let testResult: "pass" | "fail" | "timeout" = "timeout";

    const consoleHandler = (msg: any) => {
        const text = msg.text();
        logs.push(text);
        console.log(`  [WASM] ${text}`);

        if (text.includes("[PASS]")) {
            testResult = "pass";
        } else if (text.includes("[FAIL]")) {
            testResult = "fail";
        }
    };

    page.on("console", consoleHandler);

    // Reload the page
    await page.reload();

    // Wait for test to complete or timeout
    const startTime = Date.now();
    while (testResult === "timeout" && Date.now() - startTime < TIMEOUT_MS) {
        await new Promise((resolve) => setTimeout(resolve, 100));
    }

    page.off("console", consoleHandler);

    return testResult;
}

async function main() {
    const projectDir = __dirname;
    let server: ChildProcess | null = null;
    let browser: Awaited<ReturnType<typeof chromium.launch>> | null = null;

    const results: { demo: string; result: "pass" | "fail" | "timeout" | "build_error" }[] = [];

    try {
        // Start the server
        console.log("Starting server...");
        server = spawn("bun", ["server.ts"], {
            cwd: projectDir,
            stdio: ["ignore", "pipe", "pipe"],
        });

        await new Promise((resolve) => setTimeout(resolve, 1000));

        // Launch browser
        console.log("Launching browser...");
        browser = await chromium.launch({ headless: true });
        const page = await browser.newPage();

        // Initial navigation
        await page.goto(URL);

        // Run each demo
        for (const demo of DEMOS) {
            const demoName = demo.replace(".c", "");
            console.log(`\n${"=".repeat(50)}`);
            console.log(`Testing: ${demoName}`);
            console.log("=".repeat(50));

            // Update main.c to use this demo
            console.log("  Updating main.c...");
            setActiveDemo(demo, projectDir);

            // Rebuild
            console.log("  Building...");
            const buildOk = buildProject(projectDir);
            if (!buildOk) {
                console.log("  BUILD FAILED");
                results.push({ demo: demoName, result: "build_error" });
                continue;
            }

            // Run test
            console.log("  Running...");
            const result = await runTest(page, demoName);
            results.push({ demo: demoName, result });

            console.log(`  Result: ${result.toUpperCase()}`);
        }

        // Print summary
        console.log(`\n${"=".repeat(50)}`);
        console.log("SUMMARY");
        console.log("=".repeat(50));

        let passed = 0, failed = 0, errors = 0;
        for (const { demo, result } of results) {
            const icon = result === "pass" ? "✓" : result === "fail" ? "✗" : "!";
            console.log(`  ${icon} ${demo}: ${result.toUpperCase()}`);
            if (result === "pass") passed++;
            else if (result === "fail") failed++;
            else errors++;
        }

        console.log(`\nTotal: ${passed} passed, ${failed} failed, ${errors} errors`);
        process.exitCode = failed + errors > 0 ? 1 : 0;

    } finally {
        if (browser) await browser.close();
        if (server) server.kill();
    }
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});
