#!/usr/bin/luajit
--[[
    anthy-cli.lua - CLI for manipulating the Anthy user dictionary
    Copyright Jens Oliver John <base64:YXN0ZXJpc2tAMmlvbi5kZQ==>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
--]]

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

local wtypes = {
    Noun = {
        ["#T35"] = "General Noun",
        ["#T00"] = "followed by NA, SA and SURU",
        ["#T05"] = "followed by NA and SA",
        ["#T10"] = "followed by NA and SURU",
        ["#T15"] = "followed by NA",
        ["#T30"] = "followed by SURU"
    },
    ["Proper Noun"] = {
        ["#JN"] = "Personal Name",
        ["#CN"] = "Geographic Name",
        ["#KK"] = "Corporate Name"
    },
    ["Numeral"] = "#NN",
    ["Adjective"] = "#KY",
    ["Adverb"] = {
        ["#F02"] = "followed by TO and TARU",
        ["#F04"] = "followed by TO and SURU",
        ["#F06"] = "followed by TO",
        ["#F012"] = "followed by SURU"
    },
    ["Interjection"] = "#CJ",
    ["Adnominal Adjunct"] = "#RT",
    ["Single Kanji Character"] = "#KJ",
    ["Verb"] = {
        ["#K5"] = "KA 5",
        ["#G5"] = "GA 5",
        ["#S5"] = "SA 5",
        ["#T5"] = "TA 5",
        ["#N5"] = "NA 5",
        ["#B5"] = "BA 5",
        ["#M5"] = "MA 5",
        ["#R5"] = "RA 5",
        ["#W5"] = "WA 5"
    },
    ["Verb*"] = {
        ["#K5r"] = "KA 5",
        ["#G5r"] = "GA 5",
        ["#S5r"] = "SA 5",
        ["#T5r"] = "TA 5",
        ["#N5r"] = "NA 5",
        ["#B5r"] = "BA 5",
        ["#M5r"] = "MA 5",
        ["#R5r"] = "RA 5",
        ["#W5r"] = "WA 5"
    }
}

-- MAIN

local function printf(t) print(string.format(unpack(t))) end
local function eprintf(t) print("Error:"..string.format(unpack(t))) end
local function prettyprint(Cli, str)
    local p = io.popen(string.format("column -s %s -t", Cli.delim), "w")
    p:write(str)
    p:close()
end
local function reverse(t)
    local a = {}
    local b = #t + 1
    for k,v in ipairs(t) do
        a[b-k] = v
    end
    return a
end
local function remap_wt()
    local by_key = { __len = 0 }
    for cat,v in pairs(wtypes) do
        if type(v) == "table" then
            for tkey, info in pairs(v) do
                by_key[tkey] = string.format("%s:%s", cat, info)
                by_key.__len = by_key.__len + 1
            end
        else
            by_key[cat] = v
            by_key.__len = by_key.__len + 1
        end
    end
    return by_key
end
local Anthy = ffi.load("anthy")
local Anthy_version = 0
local Dictionary = { tcodes = remap_wt() }
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

-- Modification: Addition + deletion
function Dictionary:save()
    -- Additions
    if #self.data > self.data_oldlast then
        for i=self.data_oldlast+1,#self.data do
            local e = self.data[i]
            Anthy.anthy_priv_dic_add_entry(e.y, e.s, e.t, e.f)
        end
    end

    -- Deletions
    if self.deleted then
        for _,i in ipairs(self.deleted) do
            local j = 1
            Anthy.anthy_priv_dic_select_first_entry()
            while j < i do
                Anthy.anthy_priv_dic_select_next_entry()
                j = j + 1
            end
            Anthy.anthy_priv_dic_delete()
        end
    end
    return #self.data - self.data_oldlast, self.deleted and #self.deleted or 0
end

function Dictionary:match(Cli)
    local Cli = Cli
    local m = {}
    local c = {
        y = Cli.y,
        s = Cli.s,
        t = Cli.t,
        f = Cli.f and tonumber(Cli.f) or nil
    }

    for i,e in ipairs(self:entries()) do
        local flag = true
        for k,v in pairs(c) do
            if e[k] ~= c[k] then flag = false end
        end
        if flag then table.insert(m, { e, idx = i}) else flag = true end
    end

    return m
end

function Dictionary:delete(Cli)
    local Cli = Cli
    self.deleted = {}

    m = self:match(Cli)

    if #m == 0 then
        printf{ "No matching entries." }
        return false
    end
    
    for _,e in ipairs(m) do
        printf{"SELECT: %s", e[1]:tostring()}
    end

    io.read("*a")

    for _,e in ipairs(reverse(m)) do
        local i = e.idx
        table.insert(self.deleted, i)
        table.remove(self.data[i])
    end

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

function Dictionary:typestr(k)
    return self.tcodes[k]
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
    printf{[[Anthy version: %d
Dictionary entries: %d
Word type codes: %d
%s on %s (%s)
--]],
        Anthy_version, #D.data, D.tcodes.__len,
        jit.version, jit.os, jit.arch}
end

local function verb_grep(D, Cli)
    local Cli = Cli
    Cli.delim = Cli.delim or "--"
    if not Cli.format then Cli.format = "%y%C%s%C%t%C%f" end

    local m = D:match(Cli)

    -- output
    local o = ""
    for _,e in ipairs(m) do
    local p = Cli.format:gsub("%%([Cystf])", {
        y = e[1].y,
        s = e[1].s,
        f = e[1].f,
        t = D:typestr(e[1].t),
        C = Cli.delim
    })
    o = o .. p .. '\n'
    end

    prettyprint(Cli, o)

    return true
end

local function verb_delete(D, Cli)
    return D:delete(Cli)
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
       [column delimiter]   -D <string>

       <output format> := printf-like format string. Placeholders are as follows:
            %s  spelling
            %y  yomi, reading of the entry
            %t  wordtype of the entry as a human-readable string
            %T  wordtype of the entry as a Anthy-specific category code
            %f  word frequency
            %C  column delimiter, creates column for pretty-printing]])
end

-- MAIN

init()

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
    },
    {
        a = { "D", "delim" },
        f = function (t) Cli.delim = t[1] end,
        g = 1
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
elseif Cli.verb == "delete" then
    verb_delete(D, Cli)
end

local a, d = D:save()
printf{"%d addition(s), %d deletion(s)", a, d}

cleanup()
