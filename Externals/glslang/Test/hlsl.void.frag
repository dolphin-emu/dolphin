void foo1() {}
void foo2(void) {}

float4 PixelShaderFunction(float4 input) : COLOR0
{
    foo1();
    foo2();
}