#version 140

in mat3x4 m1;
in mat3x4 m2;
in float f;
in vec3 v3;
in vec4 v4;

out vec4 color;

void main()
{
    mat3x4 sum34;
    vec3 sum3;
    vec4 sum4;

    sum34 = m1 - m2;
    sum34 += m1 * f;
    sum34 += f * m1;
    sum34 /= matrixCompMult(m1, m2);
    sum34 += m1 / f;
    sum34 += f / m1;
    sum34 += f;
    sum34 -= f;

    sum3 = v4 * m2;
    sum4 = m2 * v3;

    mat4x3 m43 = transpose(sum34);
    mat4 m4 = m1 * m43;

    sum4 = v4 * m4;

    color = sum4;

//spv    if (m1 != sum34)
        ++sum34;
//    else
        --sum34;

    sum34 += mat3x4(f);
    sum34 += mat3x4(v3, f, v3, f, v3, f);

    color += sum3 * m43 + sum4;
}
