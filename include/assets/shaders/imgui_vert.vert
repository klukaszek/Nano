#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 projection;
};

void main() {
    fragTexCoord = inTexCoord;
    fragColor = inColor;
    gl_Position = projection * vec4(inPosition, 0.0, 1.0);
}
