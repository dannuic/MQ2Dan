# MQ2Dan

## MQ2DanNet
    This plugin is designed to be a serverless peer network. It is (hopefully) mostly plug and play, 
    and should automatically discover peers for most local network configurations.
    
#### Some Notes about Setup
* Some complicated network topologies won't be supported (a server interface is a better solution)
* If for some reason the peers aren't self-discovering on a local network
  * check the output of `/dinfo interface`
  * set one of the discovered interface names with `/dinfo interface <name>`
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
I have added some simple echo commands in addition to `/dinfo` and `/dobserve`, as well as a way to join/leave arbitrary groups.
* `/djoin <group>` -- join a group
* `/dleave <group>` -- leave a group
* `/dtell <name> <text>` -- echo text on peer's console
* `/dgtell <group> <text>` -- echo text on console for all peers in group
* `/dexecute <name> <command>` -- executes a command on peer's client
* `/dgexecute <group> <command>` -- executes a command on all clients in a group (except own)
* `/dgaexecute <group> <command>` -- executes a command on all clients in a group (including own)
* `/dnet [<arg>]` -- sets some variables, gives info, check  in-game output for use
* `/dquery <name> [-q <query>] [-o <result>] [-t <timeout>]` -- execute query on name and store return in result
