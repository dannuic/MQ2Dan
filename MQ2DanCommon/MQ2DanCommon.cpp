// MQ2DanCommon.cpp : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#include <MQ2Plugin.h>

#include "MQ2DanCommon.h"
#include "MQ2DanIPC.h"

#include "../MQ2DanNet/Node.h"

PLUGIN_VERSION(0.1);
PreSetup("MQ2DanCommon");
MQ2DanIPC::FileMapping<int> myTest("Test");

// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin(VOID)
{
    DebugSpewAlways("Initializing MQ2DanCommon");
    int * x = myTest;
    WriteChatf("Before: %d", *x);
    *x = (*x) + 1;

    //Add commands, MQ2Data items, hooks, etc.
    //AddCommand("/mycommand",MyCommand);
    //AddXMLFile("MQUI_MyXMLFile.xml");
    //bmMyBenchmark=AddMQ2Benchmark("My Benchmark Name");
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID)
{
    DebugSpewAlways("Shutting down MQ2DanCommon");
    int * x = myTest;
    WriteChatf("After: %d", *x);

    //Remove commands, MQ2Data items, hooks, etc.
    //RemoveMQ2Benchmark(bmMyBenchmark);
    //RemoveCommand("/mycommand");
    //RemoveXMLFile("MQUI_MyXMLFile.xml");
}

// Called after entering a new zone
PLUGIN_API VOID OnZoned(VOID)
{
    DebugSpewAlways("MQ2DanCommon::OnZoned()");
}

// Called once directly before shutdown of the new ui system, and also
// every time the game calls CDisplay::CleanGameUI()
PLUGIN_API VOID OnCleanUI(VOID)
{
    DebugSpewAlways("MQ2DanCommon::OnCleanUI()");
    // destroy custom windows, etc
}

// Called once directly after the game ui is reloaded, after issuing /loadskin
PLUGIN_API VOID OnReloadUI(VOID)
{
    DebugSpewAlways("MQ2DanCommon::OnReloadUI()");
    // recreate custom windows, etc
}

// Called every frame that the "HUD" is drawn -- e.g. net status / packet loss bar
PLUGIN_API VOID OnDrawHUD(VOID)
{
    // DONT leave in this debugspew, even if you leave in all the others
    //DebugSpewAlways("MQ2DanCommon::OnDrawHUD()");
}

// Called once directly after initialization, and then every time the gamestate changes
PLUGIN_API VOID SetGameState(DWORD GameState)
{
    DebugSpewAlways("MQ2DanCommon::SetGameState()");
    //if (GameState==GAMESTATE_INGAME)
    // create custom windows if theyre not set up, etc
}

// This is called every time MQ pulses
PLUGIN_API VOID OnPulse(VOID)
{
    // DONT leave in this debugspew, even if you leave in all the others
    //DebugSpewAlways("MQ2DanCommon::OnPulse()");
}

// This is called every time WriteChatColor is called by MQ2Main or any plugin,
// IGNORING FILTERS, IF YOU NEED THEM MAKE SURE TO IMPLEMENT THEM. IF YOU DONT
// CALL CEverQuest::dsp_chat MAKE SURE TO IMPLEMENT EVENTS HERE (for chat plugins)
PLUGIN_API DWORD OnWriteChatColor(PCHAR Line, DWORD Color, DWORD Filter)
{
    DebugSpewAlways("MQ2DanCommon::OnWriteChatColor(%s)", Line);
    return 0;
}

// This is called every time EQ shows a line of chat with CEverQuest::dsp_chat,
// but after MQ filters and chat events are taken care of.
PLUGIN_API DWORD OnIncomingChat(PCHAR Line, DWORD Color)
{
    DebugSpewAlways("MQ2DanCommon::OnIncomingChat(%s)", Line);
    return 0;
}

// This is called each time a spawn is added to a zone (inserted into EQ's list of spawns),
// or for each existing spawn when a plugin first initializes
// NOTE: When you zone, these will come BEFORE OnZoned
PLUGIN_API VOID OnAddSpawn(PSPAWNINFO pNewSpawn)
{
    DebugSpewAlways("MQ2DanCommon::OnAddSpawn(%s)", pNewSpawn->Name);
}

// This is called each time a spawn is removed from a zone (removed from EQ's list of spawns).
// It is NOT called for each existing spawn when a plugin shuts down.
PLUGIN_API VOID OnRemoveSpawn(PSPAWNINFO pSpawn)
{
    DebugSpewAlways("MQ2DanCommon::OnRemoveSpawn(%s)", pSpawn->Name);
}

// This is called each time a ground item is added to a zone
// or for each existing ground item when a plugin first initializes
// NOTE: When you zone, these will come BEFORE OnZoned
PLUGIN_API VOID OnAddGroundItem(PGROUNDITEM pNewGroundItem)
{
    DebugSpewAlways("MQ2DanCommon::OnAddGroundItem(%d)", pNewGroundItem->DropID);
}

// This is called each time a ground item is removed from a zone
// It is NOT called for each existing ground item when a plugin shuts down.
PLUGIN_API VOID OnRemoveGroundItem(PGROUNDITEM pGroundItem)
{
    DebugSpewAlways("MQ2DanCommon::OnRemoveGroundItem(%d)", pGroundItem->DropID);
}

// This is called when we receive the EQ_BEGIN_ZONE packet is received
PLUGIN_API VOID BeginZone(VOID)
{
    DebugSpewAlways("MQ2DanCommon::BeginZone");
}

// This is called when we receive the EQ_END_ZONE packet is received
PLUGIN_API VOID EndZone(VOID)
{
    DebugSpewAlways("MQ2DanCommon::EndZone");
}

// This is called when pChar!=pCharOld && We are NOT zoning
// honestly I have no idea if its better to use this one or EndZone (above)
PLUGIN_API VOID Zoned(VOID)
{
    DebugSpewAlways("MQ2DanCommon::Zoned");
}
