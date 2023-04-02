using System.Text;

public static class Decoder8086
{
    private const byte OPCODE_MOVE = 0b1000_1000;
    private const byte OPCODE_MASK = 0b1111_1100;
    private const byte D_MASK = 0b0000_0010;
    private const byte W_MASK = 0b0000_0001;
    private const byte MOD_MASK = 0b1100_0000;
    private const byte REG_MASK = 0b0011_1000;
    private const byte RM_MASK = 0b0000_0111;
    private const byte MOD_REGREG = 0b1100_0000;


    private static readonly string[] RegTable = new string[]
    {
            //byte ops (W = 0)
            "al",
            "cl",
            "dl",
            "bl",
            "ah",
            "ch",
            "dh",
            "bh",

            //word ops (W = 1)
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
        if (bytes.Length % 2 > 0)
        {
            Console.Write("Unsupported instruction. Each instruction should be 2 bytes.");
            return "";
        }

        var strBuilder = new StringBuilder();
        strBuilder.AppendLine("bits 16");
        strBuilder.AppendLine("");
        for (int i = 0; i < bytes.Length; i += 2)
        {
            var byte1 = bytes[i];
            var byte2 = bytes[i + 1];

            var opCode = byte1 & OPCODE_MASK;
            if (opCode != OPCODE_MOVE)
            {
                Console.WriteLine($"Unrecognized OPCODE: {opCode}");
                break;
            }

            var mod = byte2 & MOD_MASK;
            if (mod != MOD_REGREG)
            {
                Console.WriteLine($"Unsupported MODE: {mod}");
            }

            var d = byte1 & D_MASK;
            var w = byte1 & W_MASK;

            var reg = (byte2 & REG_MASK) >> 3;
            var rm = byte2 & RM_MASK;

            var destRegister = d > 0 ? reg : rm;
            var srcRegister = d > 0 ? rm : reg;

            //enable 4th bit (we will look at the other half of the reg table)
            if (w > 0)
            {
                destRegister |= 0b0000_1000;
                srcRegister |= 0b0000_1000;
            }

            strBuilder.AppendLine($"mov {RegTable[destRegister]}, {RegTable[srcRegister]}");
        }
        return strBuilder.ToString();
    }
}
