#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec2 fragTexCoord;

#define TEXTURE_TYPE_TEXTURE 0
#define TEXTURE_TYPE_COLOR 1

layout( push_constant ) uniform constants{
	mat4 submeshTransform;
	uint materialIndex;
	uint textureType;
} pushConstants;

layout(set = 1, binding = 0) uniform sampler2D texSampler[];

layout(set = 2, binding = 0) buffer Colors{
	vec4 color[];
} colors;

layout(location = 0) out vec4 outColor;

void main() {

	outColor = vec4(1.0, 1.0, 1.0, 1.0);

	if(pushConstants.textureType == TEXTURE_TYPE_TEXTURE){
		outColor = texture(texSampler[pushConstants.materialIndex], fragTexCoord);
	}
	else if(pushConstants.textureType == TEXTURE_TYPE_COLOR){
		outColor = colors.color[pushConstants.materialIndex];
	}
}