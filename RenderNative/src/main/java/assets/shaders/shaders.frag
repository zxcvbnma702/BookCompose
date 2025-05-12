#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler[3];

void main() {
    float y, u, v, r, g, b;
    y = texture(texSampler[0], fragTexCoord).r;
    u = texture(texSampler[1], fragTexCoord).r;
    v = texture(texSampler[2], fragTexCoord).r;
    u = u - 0.5;
    v = v - 0.5;
    r = y + 1.403 * v;
    g = y - 0.344 * u - 0.714 * v;
    b = y + 1.770 * u;
    outColor = vec4(r, g, b, 1.0);
}
