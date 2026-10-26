--
-- DanNet-TLO.lua -- MQ2DanNet TLO test suite
--
--   /lua run DanNet-TLO           all tests
--   /lua run DanNet-TLO peers     only tests whose full name matches "peers"
--
-- Run it on every client. Each instance validates that client's own view of the
-- mesh, so 6 clients = 6 independent reports.
--
-- Set EXPECTED_PEERS to the number of logged-in clients to make the peer-count
-- checks exact. Set it to nil for a single-client smoke run.
--
-- Uses the repo's u-test framework (IntegrationTests/mqTest.lua), same as
-- IntegrationTests/Me-TLO.lua.
--

local args = { ... }
local test = require('IntegrationTests.mqTest')
test.arguments(args)

local mq     = require('mq')
local DanNet = mq.TLO.DanNet

local EXPECTED_PEERS = 6

-- CreateArray() joins with '|' and FrontDelim decides which end gets it, so
-- split on '|' and drop empties instead of assuming a leading one.
local function split(list)
    local out = {}
    if type(list) ~= "string" then return out end
    for field in string.gmatch(list, "[^|]+") do out[#out + 1] = field end
    return out
end

-- Node::name() verbatim. DanNet.Name() answers with the short name unless
-- FullNames is on, and MQ's Server TLO reports the server's *full* name, not the
-- WorldServerShortname that get_full_name() uses -- so ask DanNet, not MQ.
local function node_name(full_names)
    local was = DanNet.FullNames()
    if was ~= full_names then mq.cmd(full_names and "/dnet fullnames on" or "/dnet fullnames off") end
    local name = DanNet.Name()
    mq.cmd(was and "/dnet fullnames on" or "/dnet fullnames off")
    return name
end

-- Indexed member access comes back as an MQ userdata rather than a Lua string, so
-- `==` compares identity (two prints that look the same) and a miss reads as the
-- userdata "NULL" instead of nil. Normalise before comparing.
local function str(v)
    if v == nil then return nil end
    local s = tostring(v)
    if s == "NULL" or s == "nil" then return nil end -- `and nil or` can never yield nil
    return s
end

-- Mirrors Node::is_observer_group(): prefix before the last '_' must name a known
-- peer and everything after it must be digits. Lua's greedy (.*) splits at the
-- last '_' exactly like find_last_of.
local function is_observer_group(group, peers)
    local prefix = group:match("^(.*)_(%d+)$")
    return prefix ~= nil and peers[prefix] == true
end

-- The Lua surface is `TLO.DanNet(peerName)` -> peer, plus the plain members. It
-- has no `Member[n]` bracket form, so indexing is `DanNet.Peers("1")`.
-- See mq-definitions/mq/plugins/DanNet/_TLO.lua.

-- ── plugin ────────────────────────────────────────────────────────────────────

test.plugin.Loaded = function()
    test.is_string(DanNet.Name())
end

test.plugin.Version = function()
    test.is_string(DanNet.Version())
    test.assert(#DanNet.Version() > 0)
end

-- ── identity ──────────────────────────────────────────────────────────────────
-- Name() is get_name(name()): FullNames on -> "server_char", off -> "char".
-- init_string() lowercases, so compare lowercased. Exercises the get_short_name()
-- rewrite: the short name must be the bare character name, and the full name must
-- be "<something>_<char>". Both come from DanNet itself.
test.identity.Name = function()
    local me = mq.TLO.Me.CleanName()
    test.is_string(me)

    local short = node_name(false)
    test.equal(short, me:lower())

    local full = node_name(true)
    test.assert(full:sub(-#me - 1) == ("_" .. me):lower())
    if DanNet.FullNames() then test.equal(DanNet.Name(), full) end
end

-- ── peers ─────────────────────────────────────────────────────────────────────
-- get_peers() = connected peers ∪ {self}, so a lone client reports 1.
test.peers.Count = function()
    local n = DanNet.PeerCount()
    test.is_number(n)
    test.assert(n >= 1)
    if EXPECTED_PEERS then test.equal(n, EXPECTED_PEERS) end
end

test.peers.ListMatchesCount = function()
    local peers, seen = split(DanNet.Peers()), {}
    test.equal(#peers, DanNet.PeerCount())
    for _, p in ipairs(peers) do
        test.assert(not seen[p]) -- set-backed: a duplicate means a bug
        seen[p] = true
    end
end

-- Short names come out of get_short_name(), which strips "<server>_" only when the
-- prefix matches the server. A short name may still contain underscores -- that is
-- the character's own name ("might_poknowledge") -- so test the prefix, not the
-- presence of an underscore. The prefix comes from the node's own full name.
test.peers.NameFormat = function()
    local prefix = node_name(true):match("^(.*)_.+$") .. "_"
    local full   = DanNet.FullNames()
    for _, p in ipairs(split(DanNet.Peers())) do
        if full then
            test.assert(p:find("_")) -- a full name is always "<server>_<char>"
        else
            test.assert(p:sub(1, #prefix):lower() ~= prefix:lower()) -- prefix was stripped
        end
    end
end

test.peers.Indexed = function()
    local names = split(DanNet.Peers())
    for i = 1, #names do
        test.equal(str(DanNet.Peers(tostring(i))), names[i])
    end
    test.is_nil(str(DanNet.Peers(tostring(#names + 1)))) -- range check: past end must be nil
end

-- ── groups ────────────────────────────────────────────────────────────────────
test.groups.CountMatchesList = function()
    test.equal(DanNet.GroupCount(), #split(DanNet.Groups()))
end

test.groups.Indexed = function()
    local names = split(DanNet.Groups())
    for i = 1, #names do
        test.equal(str(DanNet.Groups(tostring(i))), names[i])
    end
    test.is_nil(str(DanNet.Groups(tostring(#names + 1))))
end

-- JoinedCount() used to report _groups.size(); it must now agree with its own list.
test.joined.CountMatchesList = function()
    test.equal(DanNet.JoinedCount(), #split(DanNet.Joined()))
end

-- get_all_groups() = peer groups ∪ own groups, so everything joined is listed by Groups.
test.joined.SubsetOfGroups = function()
    local groups = {}
    for _, g in ipairs(split(DanNet.Groups())) do groups[g] = true end
    for _, g in ipairs(split(DanNet.Joined())) do
        test.assert(groups[g])
    end
end

-- With 6 clients observing each other there is always at least one real channel in
-- Joined(). If this fails, only observer groups are present -- nothing was joined.
test.joined.HasAChannel = function()
    local peers, channels = {}, 0
    for _, p in ipairs(split(DanNet.Peers())) do peers[p] = true end
    peers[DanNet.Name()] = true
    for _, g in ipairs(split(DanNet.Joined())) do
        if not is_observer_group(g, peers) then channels = channels + 1 end
    end
    test.assert(channels > 0)
end

-- ── observers ─────────────────────────────────────────────────────────────────
-- ObserveCount() counts observer entries, ObserveSet() the distinct query strings,
-- so the count can only be equal or higher (several observers may share a query).
test.observed.CountVsSet = function()
    test.assert(DanNet.ObserveCount() >= #split(DanNet.ObserveSet()))
end

-- ── config ────────────────────────────────────────────────────────────────────
-- /dnet <setting> writes the INI and then re-reads it with the one-arg ReadVar
-- overload; whatever comes back is what the TLO reports. Round-tripping catches a
-- SetVar/ReadVar key mismatch. Timeout is skipped -- it only SetVars and never
-- pushes the value to the member, so the TLO cannot observe it.
local function numeric_roundtrip(setting, member, value)
    local was = DanNet[member]()
    mq.cmd(("/dnet %s %d"):format(setting, value))
    test.equal(DanNet[member](), value)
    mq.cmd(("/dnet %s %d"):format(setting, was)) -- restore
    test.equal(DanNet[member](), was)
end

test.config.ObserveDelay = function() numeric_roundtrip("observedelay", "ObserveDelay", 9) end
test.config.Evasive      = function() numeric_roundtrip("evasive", "Evasive", 4) end
test.config.Expired      = function() numeric_roundtrip("expired", "Expired", 300) end
test.config.Keepalive    = function() numeric_roundtrip("keepalive", "Keepalive", 60) end

test.config.Debug = function()
    local was = DanNet.Debug()
    mq.cmd("/dnet debug on")
    test.is_true(DanNet.Debug())
    mq.cmd("/dnet debug off")
    test.is_false(DanNet.Debug())
    if was then mq.cmd("/dnet debug on") end
end

-- ── query ─────────────────────────────────────────────────────────────────────
-- /dquery on our own name is answered synchronously -- no network round-trip --
-- so QueryReceived() is non-zero the moment the command returns.
-- The name must be quoted: MQ2's GetArg splits on whitespace, so an unquoted
-- "eq might_poknowledge" arrives as "eq". Turning FullNames on for one read gives
-- us the exact Node::name() string instead of rebuilding it from Server/CleanName.
test.query.Self = function()
    local self_name = node_name(true)
    test.is_string(self_name)
    mq.cmd('/dquery "' .. self_name .. '" -q CleanName')
    test.not_equal(DanNet.QueryReceived(), 0)
end

test.summary()
