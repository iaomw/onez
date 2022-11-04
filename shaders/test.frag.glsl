#version 450

layout(set=0, binding=1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outputColor;

layout(location = 0) in vec4 color;

void main()
{
	//outputColor = texture(texSampler, fragTexCoord);
	outputColor = color;
}
