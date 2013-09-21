#!/usr/bin/luajit

local ffi = require("ffi")
local getopt = require("getopt")

ffi.cdef[[
extern const char* anthy_get_version_string(void);
static const int ANTHY_DIC_UTIL_OK = 0;
static const int ANTHY_DIC_UTIL_ERROR = -1;
static const int ANTHY_DIC_UTIL_DUPLICATE = -2;
static const int ANTHY_DIC_UTIL_INVALID = -3;
void anthy_dic_util_init(void);
void anthy_dic_util_quit(void);

void anthy_dic_util_set_personality(const char *);
const char *anthy_dic_util_get_anthydir(void);
int anthy_dic_util_set_encoding(int);
void anthy_priv_dic_delete(void);
int anthy_priv_dic_select_first_entry(void);
int anthy_priv_dic_select_next_entry(void);
char *anthy_priv_dic_get_index(char *buf, int len);
int anthy_priv_dic_get_freq(void);
char *anthy_priv_dic_get_wtype(char *buf, int len);
char *anthy_priv_dic_get_word(char *buf, int len);
int anthy_priv_dic_add_entry(const char *yomi, const char *word, const char *wt, int freq);
]]

-- MAIN

local function printf(t) print(string.format(unpack(t))) end
local function eprintf(t) print("Error:"..string.format(unpack(t))) end
local Anthy = ffi.load("anthy")
local Anthy_version = 0
local Dictionary = {}
local Entry = {}

function Entry:new(s, y, t, f)
    local o = {}
    setmetatable(o, { __index = Entry})
--    o.s, o.y, o.t, o.f = s, y, t, f
    o.s = s
    o.y = y
    o.t = t
    o.f = f
--    print(string.format("[++] %s %s %s %d", o.y, o.s, o.t, f))
    return o
end

function Entry:tostring()
    return string.format("y=%s -- s=%s, t=%s, f=%d", self.y, self.s, self.t, self.f)
end

function Dictionary:new()
    local o = {}
    setmetatable(o, { __index = Dictionary })
    return o
end

function Dictionary:load()
    self.data = {}
    local function _(cf)
        local buf = ffi.new("char[?]", 255)
        local buflen = ffi.new("int", 255)
        cf(buf, buflen)
        return ffi.string(buf)
    end
    local v = Anthy.anthy_priv_dic_select_first_entry()
    if v == -1 then
        return nil, "Dictionary:load(): Dictionary is empty"
    elseif v == -3 and Anthy_version >= 7716 then
        return nil, "Dictionary:load(): Could not access dictionary"
    end
    repeat
        local yomi = _(Anthy.anthy_priv_dic_get_index)
        local spelling = _(Anthy.anthy_priv_dic_get_word)
        local wtype = _(Anthy.anthy_priv_dic_get_wtype)
        local freq = self:normalize_freq(Anthy.anthy_priv_dic_get_freq())
        table.insert(self.data, Entry:new(spelling, yomi, wtype, freq))
    until Anthy.anthy_priv_dic_select_next_entry() ~= 0
    self.data_oldlast = #self.data
    return true
end

function Dictionary:normalize_freq(f)
    if f > 1000 then
        return 1000
    elseif f < 1 then
        return 1
    end
    return f
end

function Dictionary:entries()
    return self.data
end

local function usage()
    print([[anthy-cli
Copyright (C) 2013 Jens Oliver John <base64:am9qQDJpb24uZGUK>
Licensed under the GNU General Public License v3 or later.
    
Usage: anthy-cli <verb> [<verb options>]
Verbs: [add]                -a -s <spelling> -y <yomi> [-f <frequency>] [-t <type>]
       [modify]             -m <add-like filter expression> -+ <modifiers>
       [delete]             -d <add-like filter expression>
       [grep]               -g <add-like filter expression> [-F <output format>]
       [status report]      -s
       [usage]              -h
       
       <output format> := printf-like format string, anchors: %s %y %f %t]])
end

local function init()
    Anthy.anthy_dic_util_init()
    Anthy.anthy_dic_util_set_encoding(2)
    Anthy_version = tonumber(ffi.string(Anthy.anthy_get_version_string()):sub(1,-2))
end

local function cleanup()
    Anthy.anthy_dic_util_quit()
end

local function verb_status(D)
    printf{"Anthy version: %d\nDictionary entries: %d",
        Anthy_version, #D.data}
end

-- Dump
local function verb_grep(D, Cli)
    local D = D
    local Cli = Cli
    if not Cli.format then Cli.format = "%y -- %s -- %t -- %f" end
    local m = {}
    local c = {
        y = Cli.y,
        s = Cli.s,
        t = Cli.t,
        f = Cli.f and tonumber(Cli.f) or nil
    }

    -- find matches and add them to m
    for _,e in ipairs(D:entries()) do
        local flag = true
        for k,v in pairs(c) do
            if e[k] ~= c[k] then flag = false end
        end
        if flag then table.insert(m, e) else flag = true end
    end

    -- output
    local o = ""
    for _,e in ipairs(m) do
    local p = Cli.format:gsub("%%([ystf])", e)
    o = o .. p .. '\n'
    end

    io.output():write(o)

    return true
end

init()

-- PARSE THE COMMAND LINE

local Cli = {}
local noop = getopt{
    {
        a = { "h", "help" },
        f = function (t) usage(); os.exit(0) end
    },
    {
        a = { "i", "info" },
        f = function (t) Cli.verb = "info" end
    },
    {
        a = { "m", "modify" },
        f = function (t) Cli.verb = "modify" end
    },
    {
        a = { "d", "delete" },
        f = function (t) Cli.verb = "delete" end
    },
    {
        a = { "a", "add" },
        f = function (t) Cli.verb = "add" end
    },
    {
        a = { "g", "grep" },
        f = function (t) Cli.verb = "grep" end
    },
    {
        a = { "+", "mod" },
        f = function (t) Cli.mod = {} end
    },
    {
        a = { "y", "yomi" },
        f = function (t) if Cli.mod then Cli.mod.y = t[1] else Cli.y = t[1] end end,
        g = 1
    },
    {
        a = { "s", "spelling" },
        f = function (t) if Cli.mod then Cli.mod.s = t[1] else Cli.s = t[1] end end,
        g = 1
    },
    {
        a = { "f", "freq" },
        f = function (t)
            if Cli.mod then Cli.mod.f = t[1] else Cli.f = t[1] end
        end,
        g = 1
    },
    {
        a = { "t", "type" },
        f = function (t) if Cli.mod then Cli.mod.t = t[1] else Cli.t = t[1] end end,
        g = 1
    },
    {
        a = { "F", "format" },
        f = function (t) Cli.format = t[1] end,
        g = 1
    },
    {
        a = { "debug" },
        f = function () Cli.debug = true end
    }
}


local D = Dictionary.new()
local err, errmsg = D:load()

if not err then
    eprintf{errmsg}
    os.exit(1)
end

if Cli.debug then
    print("===================")
    print("Debug: pairs(t_Cli)")
    print("===================")
    for k,v in pairs(Cli) do
        print(k,v)
    end
    print("===================")
end

if Cli.verb == "info" then
    verb_status(D)
elseif Cli.verb == "grep" then
    verb_grep(D, Cli)

end

cleanup()
