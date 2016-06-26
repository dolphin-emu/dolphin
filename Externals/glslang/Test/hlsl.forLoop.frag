float4 PixelShaderFunction(float4 input) : COLOR0
{
    for (;;) ;
    for (++input; ; ) ;
    [unroll] for (; input != input; ) {}
    for (; input != input; ) { return -input; }
    for (--input; input != input; input += 2) { return -input; }
}
