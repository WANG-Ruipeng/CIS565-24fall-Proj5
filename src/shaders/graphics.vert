#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 view;
	mat4 proj;
} camera;

layout(set = 1, binding = 0) uniform ModelBufferObject {
    mat4 model;
    vec4 objectTrans;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec3 inPos = inPosition;
    if(abs(inTexCoord.x - 0.7f) < 0.001f && abs(inTexCoord.y - 0.7f) < 0.001f){
        inPos = inPos * objectTrans.w + objectTrans.xyz;
    }
    gl_Position = camera.proj * camera.view * model * vec4(inPos, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
