# RunPod Serverless Video Renderer Deployment Guide

This guide explains how to deploy your video renderer as a RunPod serverless worker that returns base64-encoded videos via a `/test_video_generator` endpoint.

## 🏗️ What Was Built

- **RunPod Handler**: Python function that runs your video renderer and returns base64-encoded video
- **Docker Image**: Ubuntu-based container with Vulkan, FFmpeg, and RunPod SDK
- **Local Testing**: Scripts to test the endpoint locally before deployment
- **Deployment Scripts**: Automated build and deployment helpers

## 📁 Files Created

```
runpod_handler.py          # Main handler function for RunPod
requirements.txt           # Python dependencies (runpod SDK)
Dockerfile.runpod          # Docker image for RunPod deployment
runpod-build.sh           # Build Docker image
runpod-test.sh            # Test locally with RunPod server
test-runpod-endpoint.py   # Test client for the endpoint
RUNPOD_DEPLOYMENT.md      # This guide
```

## 🚀 Quick Start

### 1. Build the Docker Image

```bash
./runpod-build.sh
```

This builds `video-renderer-runpod:latest` with all dependencies.

### 2. Test Locally

```bash
# Terminal 1: Start local RunPod server
./runpod-test.sh

# Terminal 2: Test the endpoint
python3 test-runpod-endpoint.py
```

The local server runs on `http://localhost:8000/run`

### 3. Deploy to RunPod

1. **Push to Docker Registry**:
   ```bash
   # Tag for your registry
   docker tag video-renderer-runpod:latest your-dockerhub-username/video-renderer-runpod:latest

   # Push to registry
   docker push your-dockerhub-username/video-renderer-runpod:latest
   ```

2. **Create RunPod Endpoint**:
   - Log into [RunPod Console](https://www.runpod.io/console/serverless)
   - Click "New Endpoint"
   - Use your Docker image: `your-dockerhub-username/video-renderer-runpod:latest`
   - Select GPU configuration (recommended: RTX 4090 or better)
   - Set environment variables if needed
   - Deploy!

## 🔧 API Usage

### Request Format

```bash
curl -X POST https://api.runpod.ai/v2/YOUR_ENDPOINT_ID/run \\
  -H "Content-Type: application/json" \\
  -H "Authorization: Bearer YOUR_API_KEY" \\
  -d '{
    "input": {
      "test": true,
      "message": "Generate video"
    }
  }'
```

### Response Format

**Success Response**:
```json
{
  "success": true,
  "video": "UklGRvj9AABXRUJQVlA4...",  // Base64-encoded MP4
  "file_size": 1234567,
  "encoding": "base64"
}
```

**Error Response**:
```json
{
  "error": "Video rendering failed: error details",
  "returncode": 1
}
```

## 🎮 Integration Example

### JavaScript/Node.js

```javascript
async function generateVideo() {
  const response = await fetch(`https://api.runpod.ai/v2/${ENDPOINT_ID}/run`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${API_KEY}`
    },
    body: JSON.stringify({
      input: { test: true }
    })
  });

  const result = await response.json();

  if (result.success) {
    // Decode base64 video
    const videoBuffer = Buffer.from(result.video, 'base64');
    // Save or stream the video
    fs.writeFileSync('output.mp4', videoBuffer);
  }
}
```

### Python

```python
import requests
import base64

def generate_video():
    response = requests.post(
        f"https://api.runpod.ai/v2/{ENDPOINT_ID}/run",
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {API_KEY}"
        },
        json={"input": {"test": True}}
    )

    result = response.json()

    if result.get("success"):
        video_data = base64.b64decode(result["video"])
        with open("output.mp4", "wb") as f:
            f.write(video_data)
```

## ⚙️ Configuration

### Environment Variables

Set these in RunPod console:

- `NVIDIA_VISIBLE_DEVICES=all` (for GPU access)
- `NVIDIA_DRIVER_CAPABILITIES=all` (for full GPU capabilities)

### GPU Requirements

- **Minimum**: RTX 3060 (8GB VRAM)
- **Recommended**: RTX 4090 (24GB VRAM)
- **Enterprise**: A100 (40GB/80GB VRAM)

### Timeout Settings

- Handler timeout: 30 seconds (configurable in `runpod_handler.py`)
- RunPod timeout: Configure in endpoint settings (recommend 60s)

## 🐛 Troubleshooting

### Common Issues

1. **"Output file not created"**
   - Check video renderer binary permissions
   - Verify shader files are present
   - Check GPU access with `nvidia-smi`

2. **"Video rendering timed out"**
   - Increase timeout in handler (currently 30s)
   - Check GPU availability
   - Monitor GPU memory usage

3. **"Connection refused"**
   - Ensure Docker image is running
   - Check port mapping (8000)
   - Verify RunPod SDK installation

### Debug Mode

Enable debug logging by editing `runpod_handler.py`:

```python
logging.basicConfig(level=logging.DEBUG)
```

### Local Testing

```bash
# Check if binary works
docker run --rm --gpus all video-renderer-runpod:latest ./video_renderer

# Check RunPod handler
docker run --rm --gpus all video-renderer-runpod:latest python3 -c "import runpod; print('OK')"
```

## 📊 Performance

### Typical Metrics

- **Cold start**: 5-15 seconds (first request)
- **Warm execution**: 2-5 seconds (subsequent requests)
- **Video generation**: 1-3 seconds (depending on complexity)
- **Base64 encoding**: <1 second

### Optimization Tips

1. **Use persistent workers** (min workers > 0) to avoid cold starts
2. **Pre-warm endpoints** with dummy requests
3. **Monitor GPU utilization** and adjust instance types
4. **Consider video compression** for smaller base64 payload

## 💰 Cost Estimation

Approximate costs per request (varies by region/GPU):

- **RTX 4090**: $0.05-0.10 per request
- **A100**: $0.15-0.25 per request
- **Cold starts**: Additional 10-30 seconds of compute

## 🔐 Security

- Store API keys securely (environment variables, secrets)
- Use HTTPS for all requests
- Consider request rate limiting
- Monitor usage and costs

## 🚀 Production Deployment

### Recommended Settings

```yaml
Endpoint Configuration:
  GPU: RTX 4090 24GB
  Max Workers: 5
  Min Workers: 1
  Timeout: 60s
  Memory: 8GB
  Storage: 20GB
```

### Monitoring

- Set up RunPod usage alerts
- Monitor endpoint health
- Track response times and error rates
- Log video generation metrics

## 📞 Support

For issues:

1. Check logs in RunPod console
2. Test locally first with `./runpod-test.sh`
3. Verify Docker image with manual testing
4. Check RunPod documentation: https://docs.runpod.io/

## 🎯 Next Steps

1. **Deploy to production** with the steps above
2. **Integrate into your application** using the API examples
3. **Monitor performance** and adjust GPU/worker settings
4. **Scale up** by adding more endpoints or workers

Happy rendering! 🎬