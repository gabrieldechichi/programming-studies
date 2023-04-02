using System.Text;
using Microsoft.VisualBasic;

public static class Decoder8086
{
    private const byte OP_MOVE_REGMEM = 0b1000_1000;
    private const byte D_MASK = 0b0000_0010;
    private const byte W_MASK = 0b0000_0001;
    private const byte REG_MASK = 0b0011_1000;
    private const byte RM_MASK = 0b0111;
    
    private const byte MOD_MASK = 0b1100_0000;

    private static readonly string[] RegTable_W0 = new string[]
    {
        "al",
        "cl",
        "dl",
        "bl",
        "ah",
        "ch",
        "dh",
        "bh",
    };

    private static readonly string[] RegTable_W1 = new string[]
    {
        "ax",
        "cx",
        "dx",
        "bx",
        "sp",
        "bp",
        "si",
        "di"
    };


    public static string DecodeInstructionStream(byte[] bytes)
    {
        var index = 0;
        var strBuilder = new StringBuilder();
        strBuilder.AppendLine("bits 16");
        strBuilder.AppendLine("");
        while (index < bytes.Length)
        {
            var byte1 = bytes[index++];

            //Register/memory to/from register
            if ((byte1 & (0xFF << 2)) == OP_MOVE_REGMEM)
            {
                var d = byte1 & D_MASK;
                var w = byte1 & W_MASK;

                var byte2 = bytes[index++];
                var mod = (byte2 & MOD_MASK) >> 6;
                var reg = (byte2 & REG_MASK) >> 3;
                var rm = byte2 & RM_MASK;

                switch (mod)
                {
                    //register to gister
                    case 0b11:
                    {
                        var regTable = w > 0 ? RegTable_W1 : RegTable_W0;
                        strBuilder.AppendLine($"mov {regTable[rm]}, {regTable[reg]}");
                        break;
                    }
                }
            }
        }

        return strBuilder.ToString();
    }
}