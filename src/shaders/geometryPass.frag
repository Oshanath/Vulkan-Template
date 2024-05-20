#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec3 WorldPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;

#define TEXTURE_TYPE_TEXTURE 0
#define TEXTURE_TYPE_COLOR 1
#define TEXTURE_TYPE_EMBEDDED 2

layout( push_constant ) uniform constants{
	mat4 submeshTransform;
	mat4 modelMatrix;
	uint materialIndex;
	uint colorIndex;
	uint textureType;
} pushConstants;

layout(set = 1, binding = 0) uniform sampler2D albedoSampler[];
layout(set = 1, binding = 1) uniform sampler2D metallicSampler[];
layout(set = 1, binding = 2) uniform sampler2D roughnessSampler[];

layout(set = 2, binding = 0) buffer Colors{
	vec4 color[];
} colors;

layout(set = 2, binding = 1) buffer Metallic{
	float color[];
} metallicColors;

layout(set = 2, binding = 2) buffer Roughness{
	float color[];
} roughnessColors;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outMetallic;
layout(location = 4) out vec4 outRoughness;

void main() {

    vec4 albedo;
    float metallic, roughness;


    if(pushConstants.textureType == TEXTURE_TYPE_TEXTURE)
    {
        albedo = vec4(texture(albedoSampler[pushConstants.materialIndex], TexCoord).rgb, 0.9);
        metallic = texture(metallicSampler[pushConstants.materialIndex], TexCoord).r;
        roughness = texture(roughnessSampler[pushConstants.materialIndex], TexCoord).r;
    }
    else if(pushConstants.textureType == TEXTURE_TYPE_COLOR)
    {
        albedo = vec4(colors.color[pushConstants.colorIndex].rgb, 0.8);
        metallic = metallicColors.color[pushConstants.colorIndex].r;
        roughness = roughnessColors.color[pushConstants.colorIndex].r;
    }
	else if(pushConstants.textureType == TEXTURE_TYPE_EMBEDDED)
    {
		albedo = vec4(texture(albedoSampler[pushConstants.materialIndex], TexCoord).rgb, 0.2);
        metallic = 0.0;
        roughness = 0.0;
	}

    outPosition = vec4(WorldPos, 1.0);
	outNormal = vec4(Normal, 1.0);
	outAlbedo = albedo;
	outMetallic = vec4(metallic);
	outRoughness = vec4(roughness);

}