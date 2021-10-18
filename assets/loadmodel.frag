#version 410

uniform vec4 color;
out vec4 outColor;

void main() {
  float depthFactor = gl_FragCoord.z - 0.15f;
  outColor = color - depthFactor;
}