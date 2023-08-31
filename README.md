# MQ2Dan

## MQ2DanNet
    This plugin is designed to be a serverless peer network. It is (hopefully) mostly plug and play, 
    and should automatically discover peers for most local network configurations.
    
#### Some Notes about Setup
* Some complicated network topologies won't be supported (a server interface is a better solution)
* If for some reason the peers aren't self-discovering on a local network
  * check the output of `/dnet interface`
  * set one of the discovered interface names with `/dnet interface <name>`
  * failing that, I'll have to look into why, so contact me with as much info as possible

### Use
There are 2 basic uses
1. Set up an observer
  * Methods of setting up an observer
    * `/dobserve <name> -q <query> [-o <result>]`
  * Reading an observer's data: `${DanNet[<name>].Observe[<query>]}` or `${DanNet[<name>].O[<query>]}`
  * Dropping an observer: `/dobserve <name> -q <query> -drop`
  * `result` is optional if no out variable is needed (or not executing from a macro)
2. Single-use direct query
  * Submitting a query: `/dquery <name> -q <query> [-o <result>] [-t <timeout>]`
    * Combines `/delay` with `/varset`
    * `timeout` is optional, and the default can be configured
    * `result` is optional, will just write out the result to `${DanNet.Q}` or `${DanNet.Query}` if omitted
    * If not run in a macro, ignores `result` and just writes out to the TLO
    

### Queries
A query is simply a normal TLO access from the perspective of the peer with the external `${}` stripped
Examples:
* `Me.CurrentMana`
* `Target.ID`
* `Me.Current$\{thing}` -- this will evaluate `${thing}` on the peer before sending a response


### Names
A fully-qualified name is `<server>_<character>`, but if you only intend to communicate on your own server, you can ommit the first part and use just `<charactername>` in all these commands.
Examples:
* Locally talk to fatty: `/dtell fatty You smell.`
* Talk to fatty on the test server: `/dtell test_fatty I can still smell you from this server!`


### Commands
* `/djoin <group> [all|save]` -- join a group, and optionally write to `[General]` or `[server_character]` (all or save, respectively)
* `/dleave <group> [all|save]` -- leave a group, and optionally write to `[General]` or `[server_character]` (all or save, respectively)
* `/dtell <name> <text>` -- echo text on peer's console
* `/dgtell <group> <text>` -- echo text on console for all peers in group
* `/dexecute <name> <command>` -- executes a command on peer's client
* `/dgexecute <group> <command>` -- executes a command on all clients in a group (except own)
* `/dggexecute <command>` -- executes a command on all clients in your current in-game group (except own)
* `/dgrexecute <command>` -- executes a command on all clients in your current in-game raid (except own)
* `/dgzexecute <command>` -- executes a command on all clients in your current in-game zone (except own)
* `/dgaexecute <group> <command>` -- executes a command on all clients in a group (including own)
* `/dggaexecute <command>` -- executes a command on all clients in your current in-game group (including own)
* `/dgraexecute <command>` -- executes a command on all clients in your current in-game raid (including own)
* `/dgzaexecute <command>` -- executes a command on all clients in your current in-game zone (including own)
* `/dnet [<arg>]` -- sets some variables, gives info, check  in-game output for use
* `/dobserve <name> [-q <query>] [-o <result>] [-drop]` -- add an observer on name and update values in result, or drop the observer
* `/dquery <name> [-q <query>] [-o <result>] [-t <timeout>]` -- execute query on name and store return in result


### EQBC -> DanNet Cheat Sheet

#### Channels vs Groups

* `/bccmd channels group1 raid1` -- requires the full list of channels any time you want to join a new channel.
  * `/djoin group1 save` -- join group1 group, store settings in MQ2DanNet.ini under `[server_character]`.  Character will automatically join this group for future sessions.
  * `/djoin tanks save` -- join tanks group, store settings in MQ2DanNet.ini under `[server_character]`.  Character will automatically join this group for future sessions.
  * `/dleave raid1 save` -- leave raid1 group, save settings in MQ2DanNet.ini under `[server_character]`.  Character will NOT automatically join this group for future sessions.

Rather than adding peers to a group manually, you can use existing commands to add a temporary in-game group/raid setup to a new DanNet group -

* `/dgga /djoin mytempgroup` -- join all peers in your current in-game group (including own) to the `mytempgroup` group.  Adding `save` will store and automatically join this group for future use.
* `/dgra /djoin mytempraid` -- join all peers in your current in-game group (including own) to the `mytempraid` group.  Adding `save` will store and automatically join this group for future use.

#### Echos

* `/bct <name> //echo something cool` -- `/dt <name> something cool`
* `/bct <channel> //echo something cool` -- `/dgt <group> something cool`

#### Commands

* `/bct <name> //command` -- `/dex <name> /command`
* `/bct <channel> //command` -- `/dge <group> /command`
* `/bcg //command` -- `/dgge /command`
* `/bcga //command` -- `/dgga /command`
* `/bcz //command` (requires netbots) -- `/dgze /command` (does NOT require netbots)
* `/bcza //command` (requires netbots) -- `/dgza /command` (does NOT require netbots)


### TLO Members
* `Name` -- current node name (fully qualified)
* `Version` -- current build version
* `Debug` -- debugging flag
* `LocalEcho` -- local echo flag (outgoing echo)
* `CommandEcho` -- command echo (incoming commands)
* `FullNames` -- print fully qualified names?
* `FrontDelim` -- use a front | in arrays?
* `Timeout` -- timeout for implicit delay in `/dquery` and `/dobserve` commands
* `ObserveDelay` -- delay between observe broadcasts (in ms)
* `Evasive` -- time to classify a peer as evasive (in ms)
* `Expired` -- keepalive time for non-responding peers (in ms)
* `Keepalive` -- keepalive time for local actor pipe (in ms)
* `PeerCount` -- number of connected peers
* `Peers` -- list of connected peers
  * `${DanNet.Peers}` -- all connected oeers
  * `${DanNet.Peers[${GroupName}]}` -- all peers in the ${GroupName} group
* `GroupCount` -- number of all groups
* `Groups` -- list of all groups (this includes hidden groups used internally! use Joined if you want only groups that are visible)
* `JoinedCount` -- number of joined groups
* `Joined` -- list of joined groups
* `O` `Observe` -- observe accessor, accessed like: `${DanNet[peer_name].Observe[query]}`
  * if no indices are specified, lists all queries observers have registered
  * if only the query is specified, list all peers that have registered that query as an observer on self
  * if only the peer is specified, list all queries that self has registered on peer
  * if fully specified, attempt to retrieve the data specified on the remote peer
* `OCount` `ObserveCount` -- count observed data on peer, or count observers on self if no peer is specified
* `OSet` `ObserveSet` -- determine if query has been set as observed data on peer, or as an observer on self if no peer specified
* `Q` `Query` -- query accessor, for last executed query

Both `Observe and `Query` are their own data types, which provide a `Received` member to determine the last received timestamp, or 0 for never received. Used like `${DanNet.Q.Received}`


### INI entries (`MQ2DanNet.ini`)
* `[General]`
  * `Groups` -- `|`-delimited list of groups for all characters to auto-join, default empty
  * `Debugging` -- on/off/true/false boolean for debugging output, default `off`
  * `Local Echo` -- on/off/true/false boolean for local echo, default `on`
  * `Command Echo` -- on/off/true/false boolean for remote and local command (`/dgex`, &c) output, default `on`
  * `Full Names` -- on/off/true/false boolean for displaying fully-qualified names (on means that all names are displayed as `server_character`), default `on`
  * `Front Delimiter` -- on/off/true/false boolean for putting the `|` at the front for the TLO output of `DanNet.Peers` &c, default `off`
  * `Query Timeout` -- timeout string for implicit delay in `/dquery` and `/dobserve`, default is `1s`
  * `Observe Delay` -- delay in milliseconds for observation evaluations to be sent, default is `1000`
  * `Evasive` -- timeout in milliseconds before a peer is considered evasive, default is `1000`
  * `Expired` -- timeout in milliseconds before an unresponsive peer is dropped, default is `30000`
  * `Keepalive` -- timeout in milliseconds to ping the main thread to keep it fresh, default is `30000`
  * `Tank` -- short-name class list to auto-join the tank channel, default is `war|pal|shd|`
  * `Priest` -- short-name class list to auto-join the priest channel, default is `clr|dru|shm|`
  * `Melee` -- short-name class list to auto-join the melee channel, default is `brd|rng|mnk|rog|bst|ber|`
  * `Caster` -- short-name class list to auto-join the caster channel, default is `nec|wiz|mag|enc|`
* `[server_character]`
  * `Groups` -- `|`-delimited list of groups for this specific character to auto-join, default empty


### Known Issues
* Proper workgroup permissions are needed for different network groups across PC's (specifically windows 10 with windows 7 machines)
* ZeroMQ has structural issues if something externally closes the TCP sockets that it is using for inter-process communication. If you are getting unexpected crashes after some time running, check your antivirus/firewall software to ensure that it's letting eqgame exist peacefully. Kaspersky is known to close these sockets.
* If you are experiencing crashes loading MQ2Dannet or when zoning, check that your Windows 10 is v1903 or greater (Build 10.0.18362). The MQNext version of MQ2Dannet uses Unix sockets for IPC. Support for this was added to Windows 10 in 2018, but *after* the v1803 public release. As a result the stack crashes when setting up network communications. 
