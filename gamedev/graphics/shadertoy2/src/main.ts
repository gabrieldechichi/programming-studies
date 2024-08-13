async function fetchShader(url: string): Promise<string> {
    const response = await fetch(url);
    if (!response.ok) {
        throw new Error(`Failed to load shader from ${url}`);
    }
    return await response.text();
}

const canvas = document.getElementById('webgl-canvas') as HTMLCanvasElement;
canvas.width = window.innerWidth;
canvas.height = window.innerHeight;

const gl = canvas.getContext('webgl2')!;
if (!gl) {
    console.error('WebGL not supported');
    throw new Error('WebGL not supported');
}

// Vertex shader program
const vsSource = `\
  #version 300 es
  in vec4 aVertexPosition;
  out vec2 vTexCoord;

  void main(void) {
    vTexCoord = aVertexPosition.xy * 0.5 + 0.5;
    gl_Position = aVertexPosition;
  }
`;

async function loadAndCompileShader() {
    const mainImageFunction = await fetchShader('shaders/main.glsl');

    const fsSource = `\
    #version 300 es
    precision highp float;

    in vec2 vTexCoord;
    uniform float iTime;
    uniform float iTimeDelta;
    uniform vec2 iResolution;

    out vec4 _fragColor;

    ${mainImageFunction}

    void main(void) {
      vec2 fragCoord = vTexCoord * iResolution;
      mainImage(_fragColor, fragCoord);
    }
  `;

    console.log(fsSource)

    const shaderProgram = initShaderProgram(gl, vsSource, fsSource);

    return {
        program: shaderProgram,
        attribLocations: {
            vertexPosition: gl.getAttribLocation(shaderProgram, 'aVertexPosition'),
        },
        uniformLocations: {
            time: gl.getUniformLocation(shaderProgram, 'iTime'),
            timeDelta: gl.getUniformLocation(shaderProgram, 'iTimeDelta'),
            resolution: gl.getUniformLocation(shaderProgram, 'iResolution'),
        },
    };
}

const programInfo = await loadAndCompileShader();
const buffers = initBuffers(gl);

function initShaderProgram(gl: WebGLRenderingContext, vsSource: string, fsSource: string): WebGLProgram {
    const vertexShader = loadShader(gl, gl.VERTEX_SHADER, vsSource);
    const fragmentShader = loadShader(gl, gl.FRAGMENT_SHADER, fsSource);

    const shaderProgram = gl.createProgram();
    gl.attachShader(shaderProgram, vertexShader);
    gl.attachShader(shaderProgram, fragmentShader);
    gl.linkProgram(shaderProgram);

    if (!gl.getProgramParameter(shaderProgram, gl.LINK_STATUS)) {
        console.error('Unable to initialize the shader program:', gl.getProgramInfoLog(shaderProgram));
        throw new Error('Unable to initialize the shader program');
    }

    return shaderProgram;
}

function loadShader(gl: WebGLRenderingContext, type: number, source: string): WebGLShader {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);

    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        console.error('An error occurred compiling the shaders:', gl.getShaderInfoLog(shader));
        gl.deleteShader(shader);
        throw new Error('An error occurred compiling the shaders');
    }

    return shader;
}

function initBuffers(gl: WebGLRenderingContext) {
    const positionBuffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);

    const positions = [
        -1.0, 1.0,
        -1.0, -1.0,
        1.0, 1.0,
        1.0, -1.0,
    ];

    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);

    return {
        position: positionBuffer,
    };
}

let previousTime = 0;

function drawScene(gl: WebGLRenderingContext, programInfo: any, buffers: any, currentTime: number) {
    // Update viewport and clear
    gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);
    gl.clearColor(0.0, 0.0, 0.0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);

    // Compute delta time
    const deltaTime = (currentTime - previousTime) / 1000;
    previousTime = currentTime;

    // Set vertex attributes
    {
        const numComponents = 2;
        const type = gl.FLOAT;
        const normalize = false;
        const stride = 0;
        const offset = 0;
        gl.bindBuffer(gl.ARRAY_BUFFER, buffers.position);
        gl.vertexAttribPointer(programInfo.attribLocations.vertexPosition, numComponents, type, normalize, stride, offset);
        gl.enableVertexAttribArray(programInfo.attribLocations.vertexPosition);
    }

    gl.useProgram(programInfo.program);

    // Update uniforms
    gl.uniform1f(programInfo.uniformLocations.time, currentTime / 1000);
    gl.uniform1f(programInfo.uniformLocations.timeDelta, deltaTime);
    gl.uniform2f(programInfo.uniformLocations.resolution, gl.canvas.width, gl.canvas.height);

    // Draw the scene
    const offset = 0;
    const vertexCount = 4;
    gl.drawArrays(gl.TRIANGLE_STRIP, offset, vertexCount);

    // Request the next frame
    requestAnimationFrame((newTime) => drawScene(gl, programInfo, buffers, newTime));
}

async function main() {
    const programInfo = await loadAndCompileShader();
    const buffers = initBuffers(gl);

    // Function to resize the canvas to fit the window
    function resizeCanvas() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
        gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);
    }

    // Add an event listener to handle window resize
    window.addEventListener('resize', resizeCanvas);

    // Call it once to set the initial size
    resizeCanvas();

    function render(currentTime: number) {
        drawScene(gl, programInfo, buffers, currentTime);
    }

    requestAnimationFrame(render);

    const socket = new WebSocket('ws://localhost:8080');

    socket.onmessage = async (event) => {
        if (event.data === 'reload-shader') {
            console.log('Reloading shader...');
            try {
                const newProgramInfo = await loadAndCompileShader();
                drawScene(gl, newProgramInfo, buffers, performance.now());
            } catch (error) {
                console.error(error);
            }
        }
    };

    socket.onopen = () => {
        console.log('WebSocket connection established');
    };

    socket.onclose = () => {
        console.log('WebSocket connection closed');
    };
}

main().catch(console.error);
