#version 410

uniform vec4 color;
out vec4 outColor;
in float depth;

void main() {
  
  outColor = color - depth;
}