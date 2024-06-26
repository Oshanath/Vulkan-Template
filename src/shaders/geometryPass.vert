#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

layout(set = 0, binding = 0) uniform ViewProjection {
    mat4 view;
    mat4 proj;
} viewProjectionUBO;

layout(set = 0, binding = 1) uniform model {
	mat4 model;
} modelUBO;

layout( push_constant ) uniform constants{
	mat4 submeshTransform;
	mat4 modelMatrix;
	uint materialIndex;
	uint colorIndex;
	uint textureType;
} pushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;


void main() {
	vec4 worldPos = pushConstants.modelMatrix * pushConstants.submeshTransform * vec4(inPosition, 1.0);
    gl_Position = viewProjectionUBO.proj * viewProjectionUBO.view * worldPos;
	fragPosition = worldPos.xyz;
	fragNormal = (pushConstants.modelMatrix * pushConstants.submeshTransform * vec4(inNormal, 0.0)).xyz;
    fragTexCoord = inTexCoord;
}