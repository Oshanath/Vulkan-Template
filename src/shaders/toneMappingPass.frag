#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable

layout(set = 0, binding = 0, rgba16f) uniform image2D image;

layout(location = 0) out vec4 outColor;

void main()
{
	ivec2 texCoord = ivec2(gl_FragCoord.xy);
	vec3 color = imageLoad(image, texCoord).xyz;
	outColor = vec4(color, 1.0);

}