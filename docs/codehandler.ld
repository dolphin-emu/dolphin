OUTPUT_FORMAT(binary)
OUTPUT(codehandler.bin)
SECTIONS
{
  . = 0x80001800;
  .text : { *(.text) }
}
