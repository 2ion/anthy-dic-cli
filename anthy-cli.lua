#!/usr/bin/luajit

local ffi = require("ffi")

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
    o.s, o.y, o.t, o.f = s, y, t, f
    return o
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
        table.insert(self.data, Entry.new(
            _(Anthy.anthy_priv_dic_get_word),
            _(Anthy.anthy_priv_dic_get_index),
            _(Anthy.anthy_priv_dic_get_wtype),
            self:normalize_freq(Anthy.anthy_priv_dic_get_freq())))
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

local function usage()
    print([[Usage: anthy-cli <verb> [<verb options>]
Verbs: [add]                -a -s <spelling> -y <yomi> [-f <frequency>] [-t <type>]
       [modify]             -m <add-like filter expression, -- := end of criteria>
       [delete]             -d <add-like filter expression>
       [grep]               -g <add-like filter expression, -- := end of criteria>
       [status report]      -s
       [usage]              -h]])
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

init()

local D = Dictionary.new()
local err, errmsg = D:load()

if not err then
    eprintf{errmsg}
else
    verb_status(D)
end

cleanup()
