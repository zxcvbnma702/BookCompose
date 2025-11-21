#version 450

layout(location = 0) out vec3 fragColor;

layout(push_constant) uniform PushConsts {
    float angle;
} pushConsts;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    float c = cos(pushConsts.angle);
    float s = sin(pushConsts.angle);
    mat2 rot = mat2(c, -s, s, c);
    
    vec2 pos = rot * positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
