var gl;

var xpos = 0.0;
var ypos = 0.0;
var blur = false;

var sceneProgram = null;
var blurHProgram = null;
var blurVProgram = null;

var textureFramebuffer;
var blurFramebuffer;
var textureTarget;
var blurTarget;

var projection = mat4.create();
var triangleVertexPositionBuffer;


function drawScene() {
  gl.bindFramebuffer(gl.FRAMEBUFFER, textureFramebuffer);
  gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

  gl.useProgram(sceneProgram);
  gl.enableVertexAttribArray(sceneProgram.vertexPositionAttribute);

  gl.uniform2f(sceneProgram.obstacle, xpos, ypos);

  gl.bindBuffer(gl.ARRAY_BUFFER, triangleVertexPositionBuffer);
  gl.vertexAttribPointer(
    sceneProgram.vertexPositionAttribute,
    triangleVertexPositionBuffer.itemSize,
    gl.FLOAT,
    false,
    0,
    0,
  );

  gl.uniformMatrix4fv(sceneProgram.projectionUniform, false, projection);
  gl.drawArrays(gl.TRIANGLES, 0, triangleVertexPositionBuffer.numItems);
  gl.bindFramebuffer(gl.FRAMEBUFFER, null);
}

function draw() {
  drawScene();

  // gl.bindFramebuffer(gl.FRAMEBUFFER, blurFramebuffer);
  // gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);
  // gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  //
  // gl.useProgram(blurHProgram);
  // gl.enableVertexAttribArray(blurHProgram.vertexPositionAttribute);
  //
  // gl.bindBuffer(gl.ARRAY_BUFFER, triangleVertexPositionBuffer);
  // gl.vertexAttribPointer(
  //   blurHProgram.vertexPositionAttribute,
  //   triangleVertexPositionBuffer.itemSize,
  //   gl.FLOAT,
  //   false,
  //   0,
  //   0,
  // );
  //
  // gl.uniformMatrix4fv(blurHProgram.projectionUniform, false, projection);
  // gl.uniform1i(blurHProgram.blur, blur);
  //
  // gl.activeTexture(gl.TEXTURE0);
  // gl.bindTexture(gl.TEXTURE_2D, textureTarget);
  // gl.uniform1i(blurHProgram.samplerUniform, 0);
  //
  // gl.drawArrays(gl.TRIANGLES, 0, triangleVertexPositionBuffer.numItems);
  // gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  //
  // gl.viewport(0, 0, gl.viewportWidth, gl.viewportHeight);
  // gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  //
  // gl.useProgram(blurVProgram);
  // gl.enableVertexAttribArray(blurVProgram.vertexPositionAttribute);
  //
  // gl.bindBuffer(gl.ARRAY_BUFFER, triangleVertexPositionBuffer);
  // gl.vertexAttribPointer(
  //   blurVProgram.vertexPositionAttribute,
  //   triangleVertexPositionBuffer.itemSize,
  //   gl.FLOAT,
  //   false,
  //   0,
  //   0,
  // );
  //
  // gl.uniformMatrix4fv(blurVProgram.projectionUniform, false, projection);
  // gl.uniform1i(blurVProgram.blur, blur);
  //
  // gl.activeTexture(gl.TEXTURE0);
  // gl.bindTexture(gl.TEXTURE_2D, blurTarget);
  // gl.uniform1i(blurVProgram.samplerUniform, 0);

  gl.drawArrays(gl.TRIANGLES, 0, triangleVertexPositionBuffer.numItems);
}

// GL main loop
function glRun() {
  if (sceneProgram != null && blurVProgram != null && blurHProgram != null) {
    draw();
  }

  requestAnimationFrame(glRun);
}

// Fetches the gl context and sets the viewport size
function glInit() {
  gl.clearColor(0.0, 0.0, 0.0, 1.0);

  initBlurShaders();
  initSceneShaders();

  initTextures();
  initBuffers();

  return true;
}

function initBlurShaders() {
  getProgram(
    "/js/webgl/lux/blurV.glsl",
    "/js/webgl/lux/blurHorizF.glsl",
    function (program) {
      if (program != null) {
        gl.useProgram(program);

        program.vertexPositionAttribute = gl.getAttribLocation(
          program,
          "position",
        );
        program.projectionUniform = gl.getUniformLocation(
          program,
          "projection",
        );
        program.samplerUniform = gl.getUniformLocation(program, "tex_sampler");
        program.blur = gl.getUniformLocation(program, "blur");

        blurHProgram = program;
      } else {
        console.error("Could not initialise horiz blur shaders");
      }
    },
  );

  getProgram(
    "/js/webgl/lux/blurV.glsl",
    "/js/webgl/lux/blurVertF.glsl",
    function (program) {
      if (program != null) {
        gl.useProgram(program);

        program.vertexPositionAttribute = gl.getAttribLocation(
          program,
          "position",
        );
        program.projectionUniform = gl.getUniformLocation(
          program,
          "projection",
        );
        program.samplerUniform = gl.getUniformLocation(program, "tex_sampler");
        program.blur = gl.getUniformLocation(program, "blur");

        blurVProgram = program;
      } else {
        console.error("Could not initialise vert blur shaders");
      }
    },
  );
}

function initSceneShaders() {
  getProgram(
    "/js/webgl/lux/sceneV.glsl",
    "/js/webgl/lux/sceneF.glsl",
    function (program) {
      if (program != null) {
        gl.useProgram(program);

        program.vertexPositionAttribute = gl.getAttribLocation(
          program,
          "position",
        );
        program.projectionUniform = gl.getUniformLocation(
          program,
          "projection",
        );
        program.obstacle = gl.getUniformLocation(program, "obstacle");

        sceneProgram = program;
      } else {
        console.error("Could not initialise scene shaders");
      }
    },
  );
}

function initTextures() {
  textureFramebuffer = gl.createFramebuffer();
  gl.bindFramebuffer(gl.FRAMEBUFFER, textureFramebuffer);

  textureTarget = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, textureTarget);
  gl.texImage2D(
    gl.TEXTURE_2D,
    0,
    gl.RGBA,
    512,
    512,
    0,
    gl.RGBA,
    gl.UNSIGNED_BYTE,
    null,
  );
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(
    gl.TEXTURE_2D,
    gl.TEXTURE_MIN_FILTER,
    gl.LINEAR_MIPMAP_NEAREST,
  );
  gl.generateMipmap(gl.TEXTURE_2D);

  gl.framebufferTexture2D(
    gl.FRAMEBUFFER,
    gl.COLOR_ATTACHMENT0,
    gl.TEXTURE_2D,
    textureTarget,
    0,
  );

  gl.bindTexture(gl.TEXTURE_2D, null);
  gl.bindRenderbuffer(gl.RENDERBUFFER, null);
  gl.bindFramebuffer(gl.FRAMEBUFFER, null);

  blurFramebuffer = gl.createFramebuffer();
  gl.bindFramebuffer(gl.FRAMEBUFFER, blurFramebuffer);

  blurTarget = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, blurTarget);
  gl.texImage2D(
    gl.TEXTURE_2D,
    0,
    gl.RGBA,
    512,
    512,
    0,
    gl.RGBA,
    gl.UNSIGNED_BYTE,
    null,
  );
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(
    gl.TEXTURE_2D,
    gl.TEXTURE_MIN_FILTER,
    gl.LINEAR_MIPMAP_NEAREST,
  );
  gl.generateMipmap(gl.TEXTURE_2D);

  gl.framebufferTexture2D(
    gl.FRAMEBUFFER,
    gl.COLOR_ATTACHMENT0,
    gl.TEXTURE_2D,
    blurTarget,
    0,
  );

  gl.bindTexture(gl.TEXTURE_2D, null);
  gl.bindRenderbuffer(gl.RENDERBUFFER, null);
  gl.bindFramebuffer(gl.FRAMEBUFFER, null);
}

function initBuffers() {
  triangleVertexPositionBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, triangleVertexPositionBuffer);
  var vertices = [
    0.0, 0.0, 1.0, 1.0, 1.0, 0.0,

    0.0, 0.0, 1.0, 1.0, 0.0, 1.0,
  ];
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
  triangleVertexPositionBuffer.itemSize = 2;
  triangleVertexPositionBuffer.numItems = 6;

  mat4.ortho(0.0, 512.0, 0.0, 512.0, 0.1, 100, projection);
}

// Starts the webgl demo
function glStart() {
  var canvas = document.getElementById("canvas");
  canvas.width = 512;
  canvas.height = 512;

  canvas.addEventListener("mousemove", function (e) {
    xpos = e.x - canvas.offsetLeft + window.scrollX;
    ypos = canvas.height - (e.y - canvas.offsetTop + window.scrollY);
  });

  canvas.addEventListener("mouseup", function (e) {
    blur = !blur;
  });

  try {
    gl = canvas.getContext("webgl");
    gl.viewportWidth = 512;
    gl.viewportHeight = 512;
  } catch (e) {}

  if (!gl) {
    console.error("Could not initialize WebGL context");
  } else {
    if (!glInit()) {
      console.error("Could not initialize WebGL application");
    } else {
      requestAnimationFrame(glRun);
    }
  }
}
