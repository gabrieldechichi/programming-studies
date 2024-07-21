
// Loads a shader from an external file
getShader = function(file, type, callback)
{
    console.log(file)
  $.ajax(
  {
    url: file,
    success: function (data)
    {
      var shader = gl.createShader(type);
      gl.shaderSource(shader, data);
      gl.compileShader(shader);

      if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
      {
          console.error(gl.getShaderInfoLog(shader));
          callback(null);
      }

      callback(shader)
    },
    error: function(req, status, err)
    {  
      console.error("Error when loading shader " + file + ": " + err);
      callback(null);
    },
    dataType: 'text'
  });
}

// Gets a shader program from an external pair of shaders
getProgram = function(vert, frag, callback)
{
  getShader(frag, gl.FRAGMENT_SHADER, function (fragment)
  {
    if (fragment != null)
    {
      getShader(vert, gl.VERTEX_SHADER, function (vertex)
      {
          program = gl.createProgram();
          gl.attachShader(program, vertex);
          gl.attachShader(program, fragment);
          gl.linkProgram(program);

          if (!gl.getProgramParameter(program, gl.LINK_STATUS))
          {
            console.error(gl.getProgramInfoLog(program));
            callback(null);
          }
          else
          {
            callback(program);
          }
      });
    }
    else
    {
      callback(null);
    }
  });
}

// Gets an image for use with textures
getImage = function(path, callback)
{
  image = new Image();
  image.onload = function() {callback(image)};
  image.src = path;
}
