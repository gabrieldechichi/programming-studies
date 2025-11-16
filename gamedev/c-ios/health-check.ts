#!/usr/bin/env bun

const ENDPOINT_ID = 'o7fl7avxbxw8x1';
const RUNPOD_API_KEY = 'rpa_UDL5BNAQG68H4I99UTHBPS1LM4R1I0AGWCZGR2WW124k0y';

async function checkHealth() {
    const url = `https://api.runpod.ai/v2/${ENDPOINT_ID}/runsync`;

    console.log('🔍 Sending health check request to RunPod...');
    console.log(`URL: ${url}`);

    try {
        const response = await fetch(url, {
            method: 'POST',
            headers: {
                'Authorization': RUNPOD_API_KEY,
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                input: {
                    endpoint: '/health'
                }
            })
        });

        if (!response.ok) {
            console.error(`❌ HTTP Error: ${response.status} ${response.statusText}`);
            const errorText = await response.text();
            console.error('Response:', errorText);
            return;
        }

        const data = await response.json();
        console.log('\n✅ Health Check Response:');
        console.log(JSON.stringify(data, null, 2));

        // Parse the actual health status from RunPod response
        if (data.output) {
            const health = data.output;
            console.log('\n📊 Health Status Summary:');
            console.log(`  • Daemon Alive: ${health.daemon_alive ? '✅' : '❌'}`);
            console.log(`  • Socket Exists: ${health.socket_exists ? '✅' : '❌'}`);
            if (health.pid) {
                console.log(`  • Process ID: ${health.pid}`);
            }
        }

    } catch (error) {
        console.error('❌ Error checking health:', error);
    }
}

// Run the health check
checkHealth();