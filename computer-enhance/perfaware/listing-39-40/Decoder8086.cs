using System.Text;

public static class Decoder8086
{
    private static readonly string[] RegTable = new string[]
    {
        //w = 0
        "al",
        "cl",
        "dl",
        "bl",
        "ah",
        "ch",
        "dh",
        "bh",

        //w = 1
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

        short readWord()
        {
            return (short)(bytes[index++] | (bytes[index++] << 8));
        }

        sbyte readByte()
        {
            return (sbyte)bytes[index++];
        }

        string decodeEffectAddressCalculation(byte b)
        {
            var mod = (b & 0b1100_0000) >> 6;
            var rm = b & 0b0111;
            var displacementOrDirectAddress = mod switch
            {
                //direct address (16 bit)
                0b00 when rm == 0b110 => readWord(),
                //8bit displacement
                0b01 => readByte(),
                //16bit displacement
                0b10 => readWord(),
                _ => 0
            };

            if (mod == 0b00 && rm == 0b110)
            {
                return $"[{displacementOrDirectAddress}]";
            }

            return displacementOrDirectAddress switch
            {
                0 => $"[{MODOperandsTable[rm]}]",
                > 0 => $"[{MODOperandsTable[rm]} + {displacementOrDirectAddress}]",
                _ => $"[{MODOperandsTable[rm]} - {-1 * displacementOrDirectAddress}]"
            };
        }

        while (index < bytes.Length)
        {
            var byte1 = bytes[index++];

            //Register/memory to/from register
            if ((byte1 & 0xFF << 2) == 0b1000_1000)
            {
                var w = byte1 & 0b0000_0001;

                var byte2 = bytes[index++];
                var mod = (byte2 & 0b1100_0000) >> 6;

                var rm = (byte2 & 0b0111) | (w << 3);
                
                //include w to correctly index into indexes 8 to 15 on RegTable
                var reg = ((byte2 & 0b0011_1000) >> 3) | (w << 3);
                //register to register
                if (mod == 0b11)
                {
                    strBuilder.AppendLine($"mov {RegTable[rm]}, {RegTable[reg]}");
                }
                else
                {
                    var d = (byte1 & 0b0000_0010) > 0;
                    var addressCalc = decodeEffectAddressCalculation(byte2);

                    strBuilder.AppendLine(d
                        ? $"mov {RegTable[reg]}, {addressCalc}"
                        : $"mov {addressCalc}, {RegTable[reg]}");
                }
            }
            //immediate to register
            else if ((byte1 & 0xFF << 4) == 0b1011_0000)
            {
                var w = (byte1 & 0b0000_1000) >> 3;
                
                //include w to correctly index into indexes 8 to 15 on RegTable
                var reg = (byte1 & 0b0000_0111) | (w << 3);

                var immediate = w > 0 ? readWord() : readByte();
                strBuilder.AppendLine($"mov {RegTable[reg]}, {immediate}");
            }
            //immediate to memory
            else if ((byte1 & 0xFF << 1) == 0b1100_0110)
            {
                var w = (byte1 & 0b0000_0001) > 0;
                var addressCalc = decodeEffectAddressCalculation(bytes[index++]);
                var immediate = w ? $"word {readWord()}" : $"byte {readByte()}";
                strBuilder.AppendLine($"mov {addressCalc}, {immediate}");
            }
            //memory to acc / acc to memory
            else if ((byte1 & 0xFF << 2) == 0b1010_0000)
            {
                var d = (byte1 & 0b0000_00010) > 0;
                var w = (byte1 & 0b0000_00001) > 0;
                var effectiveAddress = w ? readWord() : readByte();
                strBuilder.AppendLine(d
                    ? $"mov [{effectiveAddress}], ax"
                    : $"mov ax, [{effectiveAddress}]");
            }
        }

        return strBuilder.ToString();
    }
}