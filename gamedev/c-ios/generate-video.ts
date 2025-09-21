#!/usr/bin/env bun

const ENDPOINT_ID = 'o7fl7avxbxw8x1';
const RUNPOD_API_KEY = 'rpa_UDL5BNAQG68H4I99UTHBPS1LM4R1I0AGWCZGR2WW124k0y';

async function generateVideo(seconds: number = 8.33) {
    const url = `https://api.runpod.ai/v2/${ENDPOINT_ID}/runsync`;

    console.log('🎬 Sending video generation request to RunPod...');
    console.log(`Requesting ${seconds} seconds of video`);
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
                    endpoint: '/generate_video',
                    seconds: seconds
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
        console.log('\n✅ Video Generation Response:');
        // console.log(JSON.stringify(data, null, 2));

        // Parse the actual response from RunPod
        if (data.output) {
            const result = data.output;
            console.log('\n📊 Generation Summary:');
            console.log(`  • Success: ${result.success ? '✅' : '❌'}`);
            if (result.file_size) {
                console.log(`  • File Size: ${result.file_size} bytes`);
            }
            if (result.error) {
                console.log(`  • Error: ${result.error}`);
            }
        }

    } catch (error) {
        console.error('❌ Error generating video:', error);
    }
}

// Get seconds from command line argument or use default
const seconds = process.argv[2] ? parseFloat(process.argv[2]) : 8.33;

// Run the video generation
generateVideo(seconds);