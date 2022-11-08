#ifndef __cplusplus

#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_16bit_storage: require

#extension GL_EXT_shader_explicit_arithmetic_types: require
#extension GL_EXT_shader_explicit_arithmetic_types_float16: require

#extension GL_EXT_shader_explicit_arithmetic_types_int8: require
#extension GL_EXT_shader_explicit_arithmetic_types_int16: require
#extension GL_EXT_shader_explicit_arithmetic_types_int32: require

struct Vertex
{
    vec3 position;
    f16vec2 coord;
    f16vec3 normal;
};

#else



#endif
