import './style.css';
import { detectCompressedFormats, getPreferredFormat, getSupportedFormatsInPriorityOrder, type SupportedFormats } from './format-detection';
import { loadAndUploadTexture } from './texture-loader';
import { createShaderProgram, createQuadBuffers, renderQuad } from './webgl-renderer';

function displayFormatInfo(formats: SupportedFormats, infoDiv: HTMLElement) {
  let html = '<h2>Compressed Texture Support</h2>';

  html += '<div class="section">';
  html += '<strong>Available Formats:</strong><br/>';

  const formatList = [
    { key: 's3tc', format: formats.s3tc },
    { key: 's3tc_srgb', format: formats.s3tc_srgb },
    { key: 'etc', format: formats.etc },
    { key: 'etc1', format: formats.etc1 },
    { key: 'astc', format: formats.astc },
    { key: 'pvrtc', format: formats.pvrtc },
  ];

  for (const { format } of formatList) {
    const supported = format.extension !== null;
    const className = supported ? 'supported' : 'unsupported';
    const status = supported ? '✓' : '✗';
    html += `<span class="${className}">${status} ${format.name}</span><br/>`;
  }

  html += '</div>';

  const preferred = getPreferredFormat(formats);
  if (preferred) {
    html += '<div class="section">';
    html += `<strong>Selected Format:</strong> <span class="supported">${preferred.name}</span><br/>`;
    html += `<strong>File Extension:</strong> ${preferred.fileExtension}`;
    html += '</div>';
  } else {
    html += '<div class="section">';
    html += '<span class="unsupported">⚠ No compressed texture formats supported!</span>';
    html += '</div>';
  }

  infoDiv.innerHTML = html;
}

function updateTextureInfo(infoDiv: HTMLElement, width: number, height: number, mipLevels: number, internalFormat: number) {
  const textureInfo = `
    <div class="section">
      <strong>Loaded Texture Info:</strong><br/>
      Resolution: ${width}x${height}<br/>
      Mipmap Levels: ${mipLevels}<br/>
      Internal Format: 0x${internalFormat.toString(16).toUpperCase()}
    </div>
  `;
  infoDiv.innerHTML += textureInfo;
}

async function main() {
  const canvas = document.getElementById('canvas') as HTMLCanvasElement;
  const infoDiv = document.getElementById('info') as HTMLElement;

  if (!canvas || !infoDiv) {
    console.error('Canvas or info div not found');
    return;
  }

  // Create WebGL2 context
  const gl = canvas.getContext('webgl2');
  if (!gl) {
    infoDiv.innerHTML = '<span class="unsupported">WebGL2 is not supported in your browser</span>';
    return;
  }

  // Detect supported formats
  const formats = detectCompressedFormats(gl);
  displayFormatInfo(formats, infoDiv);

  const preferredFormat = getPreferredFormat(formats);
  if (!preferredFormat) {
    console.error('No compressed texture format available');
    return;
  }

  // Create shader program and buffers
  const programInfo = createShaderProgram(gl);
  if (!programInfo) {
    console.error('Failed to create shader program');
    return;
  }

  const quadBuffers = createQuadBuffers(gl, programInfo);
  if (!quadBuffers) {
    console.error('Failed to create quad buffers');
    return;
  }

  // Load and upload texture
  // This will try loading files in order of format preference
  const baseUrl = '/textures/test';
  const supportedFormats = getSupportedFormatsInPriorityOrder(formats);
  const result = await loadAndUploadTexture(gl, baseUrl, supportedFormats);

  if (!result) {
    infoDiv.innerHTML += '<div class="section"><span class="unsupported">⚠ Failed to load texture. Make sure you have compatible KTX files like: ' +
      baseUrl + '_etc2.ktx, ' + baseUrl + '_astc.ktx, etc.</span></div>';

    // Clear to a dark gray to show the canvas is working
    gl.clearColor(0.2, 0.2, 0.2, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    return;
  }

  // Update UI with texture info
  updateTextureInfo(
    infoDiv,
    result.ktxData.header.pixelWidth,
    result.ktxData.header.pixelHeight,
    result.ktxData.mipmaps.length,
    result.ktxData.header.glInternalFormat
  );

  // Update the UI to show which format was actually used
  infoDiv.innerHTML += '<div class="section"><strong>Loaded Format:</strong> <span class="supported">' +
    result.formatUsed.name + '</span></div>';

  // Render the texture
  renderQuad(gl, programInfo, quadBuffers, result.texture);

  console.log('Rendering complete!');
}

main();
