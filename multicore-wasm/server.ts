const MIME_TYPES: Record<string, string> = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.mjs': 'text/javascript',
    '.wasm': 'application/wasm',
    '.css': 'text/css',
    '.json': 'application/json',
};

const PORT = 3000;

Bun.serve({
    port: PORT,
    async fetch(req) {
        const url = new URL(req.url);
        let pathname = url.pathname === '/' ? '/index.html' : url.pathname;

        const filePath = '.' + pathname;
        const file = Bun.file(filePath);

        if (await file.exists()) {
            const ext = pathname.substring(pathname.lastIndexOf('.'));
            const contentType = MIME_TYPES[ext] || 'application/octet-stream';

            return new Response(file, {
                headers: { 'Content-Type': contentType },
            });
        }

        return new Response('Not Found', { status: 404 });
    },
});

console.log(`Server running at http://localhost:${PORT}`);
