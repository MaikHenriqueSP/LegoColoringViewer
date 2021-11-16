#version 410 core

layout(location = 0) in vec3 inPosition;

uniform float angle;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;

out vec4 fragColor;
out float depth;

void main() {
  vec4 eyePosition = viewMatrix * modelMatrix * vec4(inPosition, 1);

  depth = 1.0 - (- eyePosition.z / 2.5);  

  gl_Position = projMatrix * eyePosition;

}