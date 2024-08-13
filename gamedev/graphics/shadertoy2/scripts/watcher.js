import chokidar from 'chokidar';
import { WebSocketServer } from 'ws';

const wss = new WebSocketServer({ port: 8080 });

wss.on('connection', ws => {
  console.log('Client connected');
});

const watcher = chokidar.watch('shaders/main.glsl', {
  persistent: true
});

watcher.on('change', path => {
  console.log(`File ${path} has been changed`);
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {  // Change ws.OPEN to WebSocket.OPEN
      client.send('reload-shader');
    }
  });
});

console.log('Watcher is running...');
