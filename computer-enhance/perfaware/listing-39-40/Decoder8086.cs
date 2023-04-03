using System.Text;

public static class Decoder8086
{
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

    private static readonly string[] MODOperandsTable = new string[]
    {
        "bx + si",
        "bx + di",
        "bp + si",
        "bp + di",
        "si",
        "di",
        "bp",
        "bx"
    };

    public static string DecodeInstructionStream(byte[] bytes)
    {
        var index = 0;
        var strBuilder = new StringBuilder();
        strBuilder.AppendLine("bits 16");
        strBuilder.AppendLine("");

        ushort readWord()
        {
            return (ushort)(bytes[index++] | (bytes[index++] << 8));
        }

        ushort readByte()
        {
            return bytes[index++];
        }

        while (index < bytes.Length)
        {
            var byte1 = bytes[index++];

            //Register/memory to/from register
            if ((byte1 & 0xFF << 2) == 0b1000_1000)
            {
                var d = (byte1 & D_MASK) > 0;
                var w = (byte1 & W_MASK) > 0;

                var byte2 = bytes[index++];
                var mod = (byte2 & MOD_MASK) >> 6;
                var reg = (byte2 & REG_MASK) >> 3;
                var rm = byte2 & RM_MASK;

                var regTable = w ? RegTable_W1 : RegTable_W0;

                switch (mod)
                {
                    //no displacement
                    case 0b00:
                    {
                        //direct address, 16bit displacement
                        if (rm == 0b110)
                        {
                            var immediate = readWord();
                            strBuilder.AppendLine(d
                                ? $"mov {regTable[reg]}, [{immediate}]"
                                : $"mov [{immediate}], {regTable[reg]}");
                        }
                        else
                        {
                            strBuilder.AppendLine(d
                                ? $"mov {regTable[reg]}, [{MODOperandsTable[rm]}]"
                                : $"mov [{MODOperandsTable[rm]}], {regTable[reg]}");
                        }

                        break;
                    }
                    //8bit displacement
                    case 0b01:
                    {
                        var immediate = readByte();
                        if (immediate > 0)
                        {
                            strBuilder.AppendLine(d
                                ? $"mov {regTable[reg]}, [{MODOperandsTable[rm]} + {immediate}]"
                                : $"mov [{MODOperandsTable[rm]} + {immediate}], {regTable[reg]}");
                        }
                        else
                        {
                            strBuilder.AppendLine(d
                                ? $"mov {regTable[reg]}, [{MODOperandsTable[rm]}]"
                                : $"mov [{MODOperandsTable[rm]}], {regTable[reg]}");
                        }

                        break;
                    }
                    //16bit displacement
                    case 0b10:
                    {
                        var immediate = readWord();
                        if (immediate > 0)
                        {
                            strBuilder.AppendLine(d
                                ? $"mov {regTable[reg]}, [{MODOperandsTable[rm]} + {immediate}]"
                                : $"mov [{MODOperandsTable[rm]} + {immediate}], {regTable[reg]}");
                        }
                        else
                        {
                            strBuilder.AppendLine(d
                                ? $"mov {regTable[reg]}, [{MODOperandsTable[rm]}]"
                                : $"mov [{MODOperandsTable[rm]}], {regTable[reg]}");
                        }

                        break;
                    }
                    //register to gister
                    case 0b11:
                    {
                        strBuilder.AppendLine($"mov {regTable[rm]}, {regTable[reg]}");
                        break;
                    }
                }
            }
            //immediate to register
            else if ((byte1 & 0xFF << 4) == 0b1011_0000)
            {
                var w = (byte1 & 0b0000_1000) > 0;
                var reg = byte1 & 0b0000_0111;

                var immediate = w ? readWord() : readByte();
                var regTable = w ? RegTable_W1 : RegTable_W0;
                strBuilder.AppendLine($"mov {regTable[reg]}, {immediate}");
            }
            //immediate to memory
            else if ((byte1 & 0xFF << 1) == 0b1100_0110)
            {
                var w = (byte1 << 7) > 0;
                strBuilder.AppendLine($"immediate to memory");
            }
        }

        return strBuilder.ToString();
    }
}