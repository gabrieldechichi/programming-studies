using System.Text;

public static class Decoder8086
{
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

        string decodeEffectAddressCalculation(byte b)
        {
            var mod = (b & MOD_MASK) >> 6;
            var rm = b & RM_MASK;
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

            return displacementOrDirectAddress > 0
                ? $"[{MODOperandsTable[rm]} + {displacementOrDirectAddress}]"
                : $"[{MODOperandsTable[rm]}]";
        }

        while (index < bytes.Length)
        {
            var byte1 = bytes[index++];

            //Register/memory to/from register
            if ((byte1 & 0xFF << 2) == 0b1000_1000)
            {
                var w = (byte1 & 0b0000_0001) > 0;

                var byte2 = bytes[index++];
                var mod = (byte2 & MOD_MASK) >> 6;
                var rm = byte2 & RM_MASK;

                var regTable = w ? RegTable_W1 : RegTable_W0;

                var reg = (byte2 & REG_MASK) >> 3;
                //register to register
                if (mod == 0b11)
                {
                    strBuilder.AppendLine($"mov {regTable[rm]}, {regTable[reg]}");
                }
                else
                {
                    var d = (byte1 & 0b0000_0010) > 0;
                    var addressCalc = decodeEffectAddressCalculation(byte2);

                    strBuilder.AppendLine(d
                        ? $"mov {regTable[reg]}, {addressCalc}"
                        : $"mov {addressCalc}, {regTable[reg]}");
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