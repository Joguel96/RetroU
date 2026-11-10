#version 330
uniform sampler2D uTexture;
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = texture(uTexture, vUV) * vColor;
}
