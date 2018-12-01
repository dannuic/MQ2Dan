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
    * `/dobserve <name> <query>`
    * `${DanNet[<name>].Observe[<query>]}` or `${DanNet[<name>].O[<query>]}`
  * Reading an observer's data: `${DanNet[<name>].Observe[<query>]}` or `${DanNet[<name>].O[<query>]}`
  * Dropping an observer: `/dobserve <name> <query> drop`
2. Single-use direct query
  * Submitting a query: `/dquery <name> [-q <query>] [-o <result>] [-t <timeout>]`
    * Combines `/delay` with `/varset`
    * `timeout` is optional, and the default can be configured
    * `result` is optional, will just write out the result if omitted
    * If not run in a macro, ignores `result` and just writes out
    

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
* `/dgaexecute <group> <command>` -- executes a command on all clients in a group (including own)
* `/dggaexecute <command>` -- executes a command on all clients in your current in-game group (including own)
* `/dgraexecute <command>` -- executes a command on all clients in your current in-game raid (including own)
* `/dnet [<arg>]` -- sets some variables, gives info, check  in-game output for use
* `/dobserve <name> [-q <query>] [-o <result>] [-drop]` -- add an observer on name and update values in result, or drop the observer
* `/dquery <name> [-q <query>] [-o <result>] [-t <timeout>]` -- execute query on name and store return in result


### TLO Members
* `Name` -- current node name (fully qualified)
* `Debug` -- debugging flag
* `LocalEcho` -- local echo flag (outgoing echo)
* `CommandEcho` -- command echo (incoming commands)
* `FullNames` -- print fully qualified names?
* `FrontDelim` -- use a front | in arrays?
* `Timeout` -- timeout for implicit delay in `/dquery` and `/dobserve` commands
* `ObserveDelay` -- delay between observe broadcasts (in ms)
* `Keepalive` -- keepalive time for non-responding peers (in ms)
* `PeerCount` -- number of connected peers
* `Peers` -- list of connected peers
* `GroupCount` -- number of all groups
* `Groups` -- list of all groups
* `JoinedCount` -- number of joined groups
* `Joined` -- list of joined groups
* `O` `Observe` -- observe accessor, accessed like: `${DanNet[peer_name].Observe[query]}`
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
  * `Keepalive` -- timeout in milliseconds before an unresponsive peer is dropped, default is `30000`
  * `Tank` -- short-name class list to auto-join the tank channel, default is `war|pal|shd|`
  * `Priest` -- short-name class list to auto-join the priest channel, default is `clr|dru|shm|`
  * `Melee` -- short-name class list to auto-join the melee channel, default is `brd|rng|mnk|rog|bst|ber|`
  * `Caster` -- short-name class list to auto-join the caster channel, default is `nec|wiz|mag|enc|`
* `[server_character]`
  * `Groups` -- `|`-delimited list of groups for this specific character to auto-join, default empty


### Known Issues
* Proper workgroup permissions are needed for different network groups across PC's (specifically windows 10 with windows 7 machines)
