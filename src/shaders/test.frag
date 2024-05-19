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

layout(set = 0, binding = 2) uniform CameraLightInfo {
    vec4 camPos;
    vec4 lightDir;
} cameraLightInfo;

layout(set = 0, binding = 3) uniform Controls {
	float sunlightIntensity;
    float ambientFactor;
} controls;

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

layout(location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main() {

	if(pushConstants.textureType == TEXTURE_TYPE_TEXTURE || pushConstants.textureType == TEXTURE_TYPE_COLOR){
		
        vec3 albedo;
        float metallic, roughness;

        if(pushConstants.textureType == TEXTURE_TYPE_TEXTURE)
        {
            albedo = texture(albedoSampler[pushConstants.materialIndex], TexCoord).rgb;
            metallic = texture(metallicSampler[pushConstants.materialIndex], TexCoord).r;
            roughness = texture(roughnessSampler[pushConstants.materialIndex], TexCoord).r;
        }
        else if(pushConstants.textureType == TEXTURE_TYPE_COLOR)
        {
            albedo = colors.color[pushConstants.colorIndex].rgb;
            metallic = metallicColors.color[pushConstants.colorIndex].r;
            roughness = roughnessColors.color[pushConstants.colorIndex].r;
        }

		vec3 N = normalize(Normal);
        vec3 V = normalize(cameraLightInfo.camPos.xyz - WorldPos);

        vec3 F0 = vec3(0.04); 
        F0 = mix(F0, albedo, metallic);

        // reflectance equation
        vec3 Lo = vec3(0.0);
        for(int i = 0; i < 1; ++i) 
        {
            // calculate per-light radiance
            vec3 L = normalize(cameraLightInfo.lightDir.xyz);
            vec3 H = normalize(V + L);
            //float distance    = length(lightPositions[i] - WorldPos);
            //float attenuation = 1.0 / (distance * distance);
            //vec3 radiance     = lightColors[i] * attenuation;        
            vec3 radiance     = vec3(1.0, 1.0, 1.0) * controls.sunlightIntensity;  
        
            // cook-torrance brdf
            float NDF = DistributionGGX(N, H, roughness);        
            float G   = GeometrySmith(N, V, L, roughness);      
            vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;	  
        
            vec3 numerator    = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
            vec3 specular     = numerator / denominator;  
            
            // add to outgoing radiance Lo
            float NdotL = max(dot(N, L), 0.0);                
            Lo += (kD * albedo / PI + specular) * radiance * NdotL; 
        }   
  
        //vec3 ambient = vec3(0.03) * albedo * ao;
        vec3 ambient = vec3(controls.ambientFactor) * albedo;
        vec3 color = ambient + Lo;
	
        //color = color / (color + vec3(1.0));
        //color = pow(color, vec3(1.0/2.2));  
   
        outColor = vec4(color, 1.0);

	}
	else if(pushConstants.textureType == TEXTURE_TYPE_EMBEDDED){
		vec3 color = texture(albedoSampler[pushConstants.materialIndex], TexCoord).xyz;
        outColor = vec4(color, 1.0);
	}

}