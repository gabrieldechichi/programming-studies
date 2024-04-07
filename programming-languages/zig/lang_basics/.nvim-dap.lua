local build_cmd = "zig build"

local dap = require("dap")

dap.configurations.zig = {
    {
        name = "Launch",
        type = "codelldb",
        request = "launch",
        program = function()
            return _G.build_and_debug(build_cmd)
        end,
        cwd = "${workspaceFolder}",
        stopOnEntry = false,
        args = {}
    }
}
