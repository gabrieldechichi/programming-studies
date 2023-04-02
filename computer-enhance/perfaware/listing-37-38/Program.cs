internal class Program
{
    private static void Main(string[] args)
    {
        if (args.Length == 0)
        {
            Console.WriteLine("No file path passed as an argument");
            Environment.Exit(1);
        }

        if (args.Length != 1)
        {
            Console.WriteLine("Can't decode more than one instruction stream at a time");
            Environment.Exit(1);
        }

        var path = Path.GetFullPath(args[0]);
        if (!File.Exists(path))
        {
            Console.WriteLine($"Couldn't find file at {path}");
            Environment.Exit(1);
        }
        var bytes = File.ReadAllBytes(path);
        var decoded = Decoder8086.DecodeInstructionStream(bytes);
        Console.Write($";{args[0]}\n\n{decoded}");
    }
}