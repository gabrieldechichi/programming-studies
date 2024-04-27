package.path = package.path .. ';vendor/lick/?.lua'

lick = require('lick')
lick.reset = true

local player = {}

---@class (exact) vec2
---@field x number
---@field y number

---@param pos vec2
---@return vec2
local function to_screen(pos)
    local screen_width, screen_height = love.graphics.getDimensions()
    local screen_x = (screen_width / 2) + pos.x
    local screen_y = (screen_height / 2) - pos.y
    return { x = screen_x, y = screen_y }
end

---@param x number
---@param y number
---@return vec2
local function vec2(x, y)
    return { x = x, y = y }
end

function love.load()
    player.pos = vec2(0,0)
    player.speed = 400
end

---@param dt number
function love.update(dt)
    -- if love.keyboard.isDown("escape") then
    --     love.window.close()
    -- end

    local input = vec2(0, 0)
    if love.keyboard.isDown("d") then input.x = input.x + 1 end
    if love.keyboard.isDown("a") then input.x = input.x - 1 end
    if love.keyboard.isDown("w") then input.y = input.y + 1 end
    if love.keyboard.isDown("s") then input.y = input.y - 1 end

    player.pos.x = player.pos.x + input.x * player.speed * dt
    player.pos.y = player.pos.y + input.y * player.speed * dt
end

function love.draw()
    local player_screen = to_screen(player.pos)
    love.graphics.circle("fill", player_screen.x, player_screen.y, 100)
end
