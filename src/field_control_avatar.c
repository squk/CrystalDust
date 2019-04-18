#include "global.h"
#include "battle_setup.h"
#include "bike.h"
#include "bug_catching_contest.h"
#include "coord_event_weather.h"
#include "daycare.h"
#include "debug.h"
#include "faraway_island.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "event_scripts.h"
#include "fieldmap.h"
#include "field_control_avatar.h"
#include "field_player_avatar.h"
#include "field_poison.h"
#include "field_screen_effect.h"
#include "field_specials.h"
#include "fldeff_misc.h"
#include "item_menu.h"
#include "link.h"
#include "main.h"
#include "match_call.h"
#include "metatile_behavior.h"
#include "overworld.h"
#include "pokemon.h"
#include "safari_zone.h"
#include "script.h"
#include "secret_base.h"
#include "sound.h"
#include "start_menu.h"
#include "task.h"
#include "trainer_see.h"
#include "trainer_hill.h"
#include "wild_encounter.h"
#include "constants/bg_event_constants.h"
#include "constants/event_objects.h"
#include "constants/map_types.h"
#include "constants/maps.h"
#include "constants/songs.h"

static EWRAM_DATA u8 sWildEncounterImmunitySteps = 0;
static EWRAM_DATA u16 sPreviousPlayerMetatileBehavior = 0;

u8 gSelectedEventObject;

extern const u8 ClearPokepicAndTextboxForEarlyScriptExit[];

static void GetPlayerPosition(struct MapPosition *);
static void GetInFrontOfPlayerPosition(struct MapPosition *);
static u16 GetPlayerCurMetatileBehavior(int);
static bool8 TryStartInteractionScript(struct MapPosition*, u16, u8);
static const u8 *GetInteractionScript(struct MapPosition*, u8, u8);
static const u8 *GetInteractedEventObjectScript(struct MapPosition *, u8, u8);
static const u8 *GetInteractedBackgroundEventScript(struct MapPosition *, u8, u8);
static const u8 *GetInteractedMetatileScript(struct MapPosition *, u8, u8);
static const u8 *GetInteractedWaterScript(struct MapPosition *, u8, u8);
static bool32 TrySetupDiveDownScript(void);
static bool32 TrySetupDiveEmergeScript(void);
static bool8 TryStartStepBasedScript(struct MapPosition *, u16, u16);
static bool8 CheckStandardWildEncounter(u16);
static bool8 TryArrowWarp(struct MapPosition *, u16, u8);
static bool8 IsWarpMetatileBehavior(u16);
static bool8 IsArrowWarpMetatileBehavior(u16, u8);
static s8 GetWarpEventAtMapPosition(struct MapHeader *, struct MapPosition *);
static void SetupWarp(struct MapHeader *, s8, struct MapPosition *);
static bool8 TryDoorWarp(struct MapPosition *, u16, u8);
static s8 GetWarpEventAtPosition(struct MapHeader *, u16, u16, u8);
static u8 *GetCoordEventScriptAtPosition(struct MapHeader *, u16, u16, u8);
static struct BgEvent *GetBackgroundEventAtPosition(struct MapHeader *, u16, u16, u8);
static bool8 TryStartCoordEventScript(struct MapPosition *);
static bool8 TryStartWarpEventScript(struct MapPosition *, u16);
static bool8 TryStartMiscWalkingScripts(u16);
static bool8 TryStartStepCountScript(u16);
static void UpdateHappinessStepCounter(void);
static bool8 UpdatePoisonStepCounter(void);
static bool8 WalkingNorthOrSouthIntoSignpost(const struct MapPosition *position, u8 metatileBehavior, u8 direction);
static bool8 GetSpecialSignpostScriptId(u8 metatileBehavior, u8 direction);
static void SetupSpecialSignpostScript(const u8 *script, u8 direction);
static const u8 *GetBackgroundEventScriptForSignpost(const struct MapPosition *position);
static void Task_ShowStartMenuAfterEarlyScriptExit(u8 taskId);
static void ReplayEarlyScriptExitKeys(struct FieldInput *input, u16 *newKeys, u16 *heldKeys);

void FieldClearPlayerInput(struct FieldInput *input)
{
    input->pressedAButton = FALSE;
    input->checkStandardWildEncounter = FALSE;
    input->pressedStartButton = FALSE;
    input->pressedSelectButton = FALSE;
    input->heldDirection = FALSE;
    input->heldDirection2 = FALSE;
    input->tookStep = FALSE;
    input->pressedBButton = FALSE;
    input->input_field_1_0 = FALSE;
    input->input_field_1_1 = FALSE;
    input->input_field_1_2 = FALSE;
    input->input_field_1_3 = FALSE;
    input->dpadDirection = 0;
}

void FieldGetPlayerInput(struct FieldInput *input, u16 newKeys, u16 heldKeys)
{
    u8 tileTransitionState = gPlayerAvatar.tileTransitionState;
    u8 runningState = gPlayerAvatar.runningState;
    bool8 forcedMove = MetatileBehavior_IsForcedMovementTile(GetPlayerCurMetatileBehavior(runningState));

    if ((tileTransitionState == T_TILE_CENTER && forcedMove == FALSE) || tileTransitionState == T_NOT_MOVING)
    {
        if (GetPlayerSpeed() != 4)
        {
            if (newKeys & START_BUTTON)
                input->pressedStartButton = TRUE;
            if (newKeys & SELECT_BUTTON)
                input->pressedSelectButton = TRUE;
            if (newKeys & A_BUTTON)
                input->pressedAButton = TRUE;
            if (newKeys & B_BUTTON)
                input->pressedBButton = TRUE;
        }

        if (heldKeys & (DPAD_UP | DPAD_DOWN | DPAD_LEFT | DPAD_RIGHT))
        {
            input->heldDirection = TRUE;
            input->heldDirection2 = TRUE;
        }
    }

    if (forcedMove == FALSE)
    {
        if (tileTransitionState == T_TILE_CENTER && runningState == MOVING)
            input->tookStep = TRUE;
        if (forcedMove == FALSE && tileTransitionState == T_TILE_CENTER)
            input->checkStandardWildEncounter = TRUE;
    }

    if (heldKeys & DPAD_UP)
        input->dpadDirection = DIR_NORTH;
    else if (heldKeys & DPAD_DOWN)
        input->dpadDirection = DIR_SOUTH;
    else if (heldKeys & DPAD_LEFT)
        input->dpadDirection = DIR_WEST;
    else if (heldKeys & DPAD_RIGHT)
        input->dpadDirection = DIR_EAST;
        
#ifdef DEBUG
    if ((heldKeys & R_BUTTON) && input->pressedStartButton)
    {
        input->input_field_1_2 = TRUE;
        input->pressedStartButton = FALSE;
    }
#endif
}

int ProcessPlayerFieldInput(struct FieldInput *input)
{
    struct MapPosition position;
    u8 playerDirection;
    u16 metatileBehavior;

    gSpecialVar_LastTalked = 0;
    gSelectedEventObject = 0;
    gSpecialVar_TextColor = 0xFF;
    TextboxUseStandardBorder();

    playerDirection = GetPlayerFacingDirection();
    GetPlayerPosition(&position);
    metatileBehavior = MapGridGetMetatileBehaviorAt(position.x, position.y);

    if (CheckBugCatchingContestTimerExpired())
        return TRUE;

    if (CheckForTrainersWantingBattle() == TRUE)
        return TRUE;

    if (TryRunOnFrameMapScript() == TRUE)
        return TRUE;

    if (input->pressedBButton && TrySetupDiveEmergeScript() == TRUE)
        return TRUE;
    if (input->tookStep)
    {
        IncrementGameStat(GAME_STAT_STEPS);
        IncrementBirthIslandRockStepCount();
        if (TryStartStepBasedScript(&position, metatileBehavior, playerDirection) == TRUE)
            return TRUE;
    }
    if (input->checkStandardWildEncounter)
    {
        if (input->dpadDirection == 0 || input->dpadDirection == playerDirection)
        {
            GetInFrontOfPlayerPosition(&position);
            metatileBehavior = MapGridGetMetatileBehaviorAt(position.x, position.y);

            if (WalkingNorthOrSouthIntoSignpost(&position, metatileBehavior, playerDirection) == TRUE)
                return TRUE;

            GetPlayerPosition(&position);
            metatileBehavior = MapGridGetMetatileBehaviorAt(position.x, position.y);
        }
        if (input->checkStandardWildEncounter && CheckStandardWildEncounter(metatileBehavior) == TRUE)
            return TRUE;
    }
    if (input->heldDirection && input->dpadDirection == playerDirection)
    {
        if (TryArrowWarp(&position, metatileBehavior, playerDirection) == TRUE)
            return TRUE;
    }

    GetInFrontOfPlayerPosition(&position);
    metatileBehavior = MapGridGetMetatileBehaviorAt(position.x, position.y);

    if (input->heldDirection && input->dpadDirection == playerDirection)
    {
        if (WalkingNorthOrSouthIntoSignpost(&position, metatileBehavior, playerDirection) == TRUE)
            return TRUE;
    }

    if (input->pressedAButton && TryStartInteractionScript(&position, metatileBehavior, playerDirection) == TRUE)
        return TRUE;

    if (input->heldDirection2 && input->dpadDirection == playerDirection)
    {
        if (TryDoorWarp(&position, metatileBehavior, playerDirection) == TRUE)
            return TRUE;
    }
    if (input->pressedAButton && TrySetupDiveDownScript() == TRUE)
        return TRUE;
    if (input->pressedStartButton)
    {
        PlaySE(SE_WIN_OPEN);
        ShowStartMenu();
        return TRUE;
    }
    if (input->pressedSelectButton && UseRegisteredKeyItemOnField() == TRUE)
        return TRUE;
#ifdef DEBUG
    if (input->input_field_1_2)
    {
        PlaySE(SE_WIN_OPEN);
        ShowDebugMenu();
        return TRUE;
    }
#endif

    return FALSE;
}

void CheckEarlyScriptExit(struct FieldInput *input)
{
    if (ScriptContext1_IsScriptSetUp() != TRUE)
        return;

    if (gExitFromScriptEarlyWaitTimer != 0)
    {
        gExitFromScriptEarlyWaitTimer--;
        return;
    }

    if (CanExitScriptEarly() != TRUE)
        return;
    
    if (input->dpadDirection != DIR_NONE && GetPlayerFacingDirection() != input->dpadDirection)
    {
        ScriptContext1_SetupScript(ClearPokepicAndTextboxForEarlyScriptExit);
        ScriptContext2_Enable();
    }
    else if (input->pressedStartButton)
    {
        ScriptContext1_SetupScript(ClearPokepicAndTextboxForEarlyScriptExit);
        ScriptContext2_Enable();
        if (!FuncIsActiveTask(Task_ShowStartMenuAfterEarlyScriptExit))
            CreateTask(Task_ShowStartMenuAfterEarlyScriptExit, 8);
    }
}

static void Task_ShowStartMenuAfterEarlyScriptExit(u8 taskId)
{
    if (!ScriptContext2_IsEnabled())
    {
        PlaySE(SE_WIN_OPEN);
        ShowStartMenu();
        DestroyTask(taskId);
    }
}

static void GetPlayerPosition(struct MapPosition *position)
{
    PlayerGetDestCoords(&position->x, &position->y);
    position->height = PlayerGetZCoord();
}

static void GetInFrontOfPlayerPosition(struct MapPosition *position)
{
    s16 x, y;

    GetXYCoordsOneStepInFrontOfPlayer(&position->x, &position->y);
    PlayerGetDestCoords(&x, &y);
    if (MapGridGetZCoordAt(x, y) != 0)
        position->height = PlayerGetZCoord();
    else
        position->height = 0;
}

static u16 GetPlayerCurMetatileBehavior(int runningState)
{
    s16 x, y;

    PlayerGetDestCoords(&x, &y);
    return MapGridGetMetatileBehaviorAt(x, y);
}

static bool8 TryStartInteractionScript(struct MapPosition *position, u16 metatileBehavior, u8 direction)
{
    const u8 *script = GetInteractionScript(position, metatileBehavior, direction);
    if (script == NULL)
        return FALSE;

    // Don't play interaction sound for certain scripts.
    if (script != EventScript_PlayerPC
     && script != EventScript_SecretBasePC
     && script != EventScript_RecordMixingSecretBasePC
     && script != SecretBase_EventScript_DollInteract
     && script != SecretBase_EventScript_CushionInteract
     && script != EventScript_PC)
        PlaySE(SE_SELECT);

    ScriptContext1_SetupScript(script);
    return TRUE;
}

static const u8 *GetInteractionScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    const u8 *script = GetInteractedEventObjectScript(position, metatileBehavior, direction);
    if (script != NULL)
        return script;

    script = GetInteractedBackgroundEventScript(position, metatileBehavior, direction);
    if (script != NULL)
        return script;

    script = GetInteractedMetatileScript(position, metatileBehavior, direction);
    if (script != NULL)
        return script;

    script = GetInteractedWaterScript(position, metatileBehavior, direction);
    if (script != NULL)
        return script;

    return NULL;
}

const u8 *GetInteractedLinkPlayerScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    u8 eventObjectId;
    s32 i;

    if (!MetatileBehavior_IsCounter(MapGridGetMetatileBehaviorAt(position->x, position->y)))
        eventObjectId = GetEventObjectIdByXYZ(position->x, position->y, position->height);
    else
        eventObjectId = GetEventObjectIdByXYZ(position->x + gDirectionToVectors[direction].x, position->y + gDirectionToVectors[direction].y, position->height);

    if (eventObjectId == EVENT_OBJECTS_COUNT || gEventObjects[eventObjectId].localId == EVENT_OBJ_ID_PLAYER)
        return NULL;

    for (i = 0; i < 4; i++)
    {
        if (gLinkPlayerEventObjects[i].active == TRUE && gLinkPlayerEventObjects[i].eventObjId == eventObjectId)
            return NULL;
    }

    gSelectedEventObject = eventObjectId;
    gSpecialVar_LastTalked = gEventObjects[eventObjectId].localId;
    gSpecialVar_Facing = direction;
    return GetEventObjectScriptPointerByEventObjectId(eventObjectId);
}

static const u8 *GetInteractedEventObjectScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    u8 eventObjectId;
    const u8 *script;

    eventObjectId = GetEventObjectIdByXYZ(position->x, position->y, position->height);
    if (eventObjectId == EVENT_OBJECTS_COUNT || gEventObjects[eventObjectId].localId == EVENT_OBJ_ID_PLAYER)
    {
        if (MetatileBehavior_IsCounter(metatileBehavior) != TRUE)
            return NULL;

        // Look for an event object on the other side of the counter.
        eventObjectId = GetEventObjectIdByXYZ(position->x + gDirectionToVectors[direction].x, position->y + gDirectionToVectors[direction].y, position->height);
        if (eventObjectId == EVENT_OBJECTS_COUNT || gEventObjects[eventObjectId].localId == EVENT_OBJ_ID_PLAYER)
            return NULL;
    }

    gSelectedEventObject = eventObjectId;
    gSpecialVar_LastTalked = gEventObjects[eventObjectId].localId;
    gSpecialVar_Facing = direction;

    if (InTrainerHill() == TRUE)
        script = sub_81D62AC();
    else
        script = GetEventObjectScriptPointerByEventObjectId(eventObjectId);

    script = GetRamScript(gSpecialVar_LastTalked, script);
    return script;
}

static const u8 *GetInteractedBackgroundEventScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    u8 whichScript;
    struct BgEvent *bgEvent = GetBackgroundEventAtPosition(&gMapHeader, position->x - 7, position->y - 7, position->height);

    if (bgEvent == NULL)
        return NULL;
    if (bgEvent->bgUnion.script == NULL)
        return EventScript_TestSignpostMsg;

    whichScript = GetSpecialSignpostScriptId(metatileBehavior, direction);

    switch (bgEvent->kind)
    {
    case BG_EVENT_PLAYER_FACING_NORTH:
        if (direction != DIR_NORTH)
            return NULL;
        break;
    case BG_EVENT_PLAYER_FACING_SOUTH:
        if (direction != DIR_SOUTH)
            return NULL;
        break;
    case BG_EVENT_PLAYER_FACING_EAST:
        if (direction != DIR_EAST)
            return NULL;
        break;
    case BG_EVENT_PLAYER_FACING_WEST:
        if (direction != DIR_WEST)
            return NULL;
        break;
    case 5:
    case 6:
    case BG_EVENT_HIDDEN_ITEM:
        gSpecialVar_0x8004 = ((u32)bgEvent->bgUnion.script >> 16) + FLAG_HIDDEN_ITEMS_START;
        gSpecialVar_0x8005 = (u32)bgEvent->bgUnion.script;
        if (FlagGet(gSpecialVar_0x8004) == TRUE)
            return NULL;
        gSpecialVar_Facing = direction;
        return EventScript_HiddenItemScript;
    case BG_EVENT_SECRET_BASE:
        if (direction == DIR_NORTH)
        {
            gSpecialVar_0x8004 = bgEvent->bgUnion.secretBaseId;
            if (TrySetCurSecretBase())
                return EventScript_2759F1;
        }
        return NULL;
    }

    if (whichScript != 0xFF)
        TextboxUseSignBorder();

    gSpecialVar_Facing = direction;
    return bgEvent->bgUnion.script;
}

static const u8 *GetInteractedMetatileScript(struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    s8 height;

    if (MetatileBehavior_IsPlayerFacingTVScreen(metatileBehavior, direction) == TRUE)
        return EventScript_TV;
    if (MetatileBehavior_IsPC(metatileBehavior) == TRUE)
        return EventScript_PC;
    if (MetatileBehavior_IsClosedSootopolisDoor(metatileBehavior) == TRUE)
        return EventScript_ClosedSootopolisDoor;
    if (MetatileBehavior_IsUnknownClosedDoor(metatileBehavior) == TRUE)
        return SkyPillar_Outside_EventScript_2393F9;
    if (MetatileBehavior_IsCableBoxResults1(metatileBehavior) == TRUE)
        return EventScript_CableBoxResults;
    if (MetatileBehavior_IsPokeblockFeeder(metatileBehavior) == TRUE)
        return EventScript_PokeBlockFeeder;
    if (MetatileBehavior_IsTrickHousePuzzleDoor(metatileBehavior) == TRUE)
        return Route110_TrickHouseEntrance_EventScript_26A22A;
    if (MetatileBehavior_IsRegionMap(metatileBehavior) == TRUE)
        return EventScript_RegionMap;
    if (MetatileBehavior_IsPictureBookShelf(metatileBehavior) == TRUE)
        return EventScript_PictureBookShelf;
    if (MetatileBehavior_IsBookShelf(metatileBehavior) == TRUE)
        return EventScript_BookShelf;
    if (MetatileBehavior_IsPokeCenterBookShelf(metatileBehavior) == TRUE)
        return EventScript_PokemonCenterBookShelf;
    if (MetatileBehavior_IsVase(metatileBehavior) == TRUE)
        return EventScript_Vase;
    if (MetatileBehavior_IsTrashCan(metatileBehavior) == TRUE)
        return EventScript_EmptyTrashCan;
    if (MetatileBehavior_IsShopShelf(metatileBehavior) == TRUE)
        return EventScript_ShopShelf;
    if (MetatileBehavior_IsBlueprint(metatileBehavior) == TRUE)
        return EventScript_Blueprint;
    if (MetatileBehavior_IsPlayerFacingWirelessBoxResults(metatileBehavior, direction) == TRUE)
        return EventScript_WirelessBoxResults;
    if (MetatileBehavior_IsCableBoxResults2(metatileBehavior, direction) == TRUE)
        return EventScript_CableBoxResults;
    if (MetatileBehavior_IsQuestionnaire(metatileBehavior) == TRUE)
        return EventScript_Questionnaire;
    if (MetatileBehavior_IsTrainerHillTimer(metatileBehavior) == TRUE)
        return EventScript_TrainerHillTimer;
    if (MetatileBehavior_IsWindow(metatileBehavior) == TRUE)
        return EventScript_Window;
    if (MetatileBehavior_IsRadio(metatileBehavior) == TRUE)
        return EventScript_Radio1;

    height = position->height;
    if (height == MapGridGetZCoordAt(position->x, position->y))
    {
        if (MetatileBehavior_IsSecretBasePC(metatileBehavior) == TRUE)
            return EventScript_SecretBasePC;
        if (MetatileBehavior_IsRecordMixingSecretBasePC(metatileBehavior) == TRUE)
            return EventScript_RecordMixingSecretBasePC;
        if (MetatileBehavior_IsSecretBaseSandOrnament(metatileBehavior) == TRUE)
            return EventScript_SecretBaseSandOrnament;
        if (MetatileBehavior_IsSecretBaseShieldOrToyTV(metatileBehavior) == TRUE)
            return EventScript_SecretBaseShieldOrToyTV;
        if (MetatileBehavior_IsMB_C6(metatileBehavior) == TRUE)
        {
            SetSecretBaseSecretsTvFlags_MiscFurnature();
            return NULL;
        }
        if (MetatileBehavior_HoldsLargeDecoration(metatileBehavior) == TRUE)
        {
            SetSecretBaseSecretsTvFlags_LargeDecorationSpot();
            return NULL;
        }
        if (MetatileBehavior_HoldsSmallDecoration(metatileBehavior) == TRUE)
        {
            SetSecretBaseSecretsTvFlags_SmallDecorationSpot();
            return NULL;
        }
    }
    else if (MetatileBehavior_IsSecretBasePoster(metatileBehavior) == TRUE)
    {
        SetSecretBaseSecretsTvFlags_Poster();
        return NULL;
    }

    return NULL;
}

static const u8 *GetInteractedWaterScript(struct MapPosition *unused1, u8 metatileBehavior, u8 direction)
{
    if (FlagGet(FLAG_BADGE05_GET) == TRUE && PartyHasMonWithSurf() == TRUE && IsPlayerFacingSurfableFishableWater() == TRUE)
        return EventScript_UseSurf;

    if (MetatileBehavior_IsWaterfall(metatileBehavior) == TRUE)
    {
        if (FlagGet(FLAG_BADGE08_GET) == TRUE && IsPlayerSurfingNorth() == TRUE)
            return EventScript_UseWaterfall;
        else
            return EventScript_CannotUseWaterfall;
    }
    return NULL;
}

static bool32 TrySetupDiveDownScript(void)
{
    if (FlagGet(FLAG_BADGE07_GET) && TrySetDiveWarp() == 2)
    {
        ScriptContext1_SetupScript(EventScript_UseDive);
        return TRUE;
    }
    return FALSE;
}

static bool32 TrySetupDiveEmergeScript(void)
{
    if (FlagGet(FLAG_BADGE07_GET) && gMapHeader.mapType == MAP_TYPE_UNDERWATER && TrySetDiveWarp() == 1)
    {
        ScriptContext1_SetupScript(EventScript_UseDiveUnderwater);
        return TRUE;
    }
    return FALSE;
}

static bool8 TryStartStepBasedScript(struct MapPosition *position, u16 metatileBehavior, u16 direction)
{
    if (TryStartCoordEventScript(position) == TRUE)
        return TRUE;
    if (TryStartWarpEventScript(position, metatileBehavior) == TRUE)
        return TRUE;
    if (TryStartMiscWalkingScripts(metatileBehavior) == TRUE)
        return TRUE;
    if (TryStartStepCountScript(metatileBehavior) == TRUE)
        return TRUE;
    if (UpdateRepelCounter() == TRUE)
        return TRUE;
    return FALSE;
}

static bool8 TryStartCoordEventScript(struct MapPosition *position)
{
    u8 *script = GetCoordEventScriptAtPosition(&gMapHeader, position->x - 7, position->y - 7, position->height);

    if (script == NULL)
        return FALSE;
    ScriptContext1_SetupScript(script);
    return TRUE;
}

static bool8 TryStartMiscWalkingScripts(u16 metatileBehavior)
{
    s16 x, y;

    if (MetatileBehavior_IsCrackedFloorHole(metatileBehavior))
    {
        ScriptContext1_SetupScript(EventScript_FallDownHole);
        return TRUE;
    }
    else if (MetatileBehavior_IsBattlePyramidWarp(metatileBehavior))
    {
        ScriptContext1_SetupScript(BattleFrontier_BattlePyramidEmptySquare_EventScript_252BE8);
        return TRUE;
    }
    else if (MetatileBehavior_IsSecretBaseGlitterMat(metatileBehavior) == TRUE)
    {
        DoSecretBaseGlitterMatSparkle();
        return FALSE;
    }
    else if (MetatileBehavior_IsSecretBaseSoundMat(metatileBehavior) == TRUE)
    {
        PlayerGetDestCoords(&x, &y);
        PlaySecretBaseMusicNoteMatSound(MapGridGetMetatileIdAt(x, y));
        return FALSE;
    }
    return FALSE;
}

static bool8 TryStartStepCountScript(u16 metatileBehavior)
{
    if (InUnionRoom() == TRUE)
    {
        return FALSE;
    }

    IncrementRematchStepCounter();
    UpdateHappinessStepCounter();
    UpdateFarawayIslandStepCounter();

    if (!(gPlayerAvatar.flags & PLAYER_AVATAR_FLAG_6) && !MetatileBehavior_IsForcedMovementTile(metatileBehavior))
    {
        if (UpdatePoisonStepCounter() == TRUE)
        {
            ScriptContext1_SetupScript(EventScript_Poison);
            return TRUE;
        }
        if (ShouldEggHatch())
        {
            IncrementGameStat(GAME_STAT_HATCHED_EGGS);
            ScriptContext1_SetupScript(EventScript_EggHatch);
            return TRUE;
        }
        if (UnusualWeatherHasExpired() == TRUE)
        {
            ScriptContext1_SetupScript(UnusualWeather_EventScript_EndEventAndCleanup_1);
            return TRUE;
        }
        if (ShouldDoBrailleRegicePuzzle() == TRUE)
        {
            ScriptContext1_SetupScript(IslandCave_EventScript_238EAF);
            return TRUE;
        }
        if (ShouldDoWallyCall() == TRUE)
        {
            ScriptContext1_SetupScript(MauvilleCity_EventScript_1DF7BA);
            return TRUE;
        }
        if (ShouldDoWinonaCall() == TRUE)
        {
            ScriptContext1_SetupScript(Route119_EventScript_1F49EC);
            return TRUE;
        }
        if (ShouldDoScottCall() == TRUE)
        {
            //ScriptContext1_SetupScript(NewBarkTown_ProfessorElmsLab_EventScript_1FA4D6);
            return TRUE;
        }
        if (ShouldDoRoxanneCall() == TRUE)
        {
            ScriptContext1_SetupScript(RustboroCity_Gym_EventScript_21307B);
            return TRUE;
        }
        if (ShouldDoRivalRayquazaCall() == TRUE)
        {
            ScriptContext1_SetupScript(MossdeepCity_SpaceCenter_2F_EventScript_224175);
            return TRUE;
        }
    }

    if (SafariZoneTakeStep() == TRUE)
        return TRUE;
    if (CountSSTidalStep(1) == TRUE)
    {
        ScriptContext1_SetupScript(SSTidalCorridor_EventScript_23C050);
        return TRUE;
    }
    if (TryStartMatchCall())
        return TRUE;
    return FALSE;
}

void Unref_ClearHappinessStepCounter(void)
{
    VarSet(VAR_HAPPINESS_STEP_COUNTER, 0);
}

static void UpdateHappinessStepCounter(void)
{
    u16 *ptr = GetVarPointer(VAR_HAPPINESS_STEP_COUNTER);
    int i;

    (*ptr)++;
    (*ptr) %= 128;
    if (*ptr == 0)
    {
        struct Pokemon *mon = gPlayerParty;
        for (i = 0; i < PARTY_SIZE; i++)
        {
            AdjustFriendship(mon, FRIENDSHIP_EVENT_WALKING);
            mon++;
        }
    }
}

void ClearPoisonStepCounter(void)
{
    VarSet(VAR_POISON_STEP_COUNTER, 0);
}

static bool8 UpdatePoisonStepCounter(void)
{
    u16 *ptr;

    if (gMapHeader.mapType != MAP_TYPE_SECRET_BASE)
    {
        ptr = GetVarPointer(VAR_POISON_STEP_COUNTER);
        (*ptr)++;
        (*ptr) %= 4;
        if (*ptr == 0)
        {
            switch (DoPoisonFieldEffect())
            {
            case 0:
                return FALSE;
            case 1:
                return FALSE;
            case 2:
                return TRUE;
            }
        }
    }
    return FALSE;
}

void RestartWildEncounterImmunitySteps(void)
{
    // Starts at 0 and counts up to 4 steps.
    sWildEncounterImmunitySteps = 0;
}

static bool8 CheckStandardWildEncounter(u16 metatileBehavior)
{
    if (sWildEncounterImmunitySteps < 4)
    {
        sWildEncounterImmunitySteps++;
        sPreviousPlayerMetatileBehavior = metatileBehavior;
        return FALSE;
    }

    if (StandardWildEncounter(metatileBehavior, sPreviousPlayerMetatileBehavior) == TRUE)
    {
        sWildEncounterImmunitySteps = 0;
        sPreviousPlayerMetatileBehavior = metatileBehavior;
        return TRUE;
    }

    sPreviousPlayerMetatileBehavior = metatileBehavior;
    return FALSE;
}

static bool8 WalkingNorthOrSouthIntoSignpost(const struct MapPosition *position, u8 metatileBehavior, u8 direction)
{
    const u8 *script;

    if (gMain.heldKeys & (DPAD_RIGHT | DPAD_LEFT))
        return FALSE;

    if (direction == DIR_WEST || direction == DIR_EAST)
        return FALSE;

    switch (GetSpecialSignpostScriptId(metatileBehavior, direction))
    {
        case 0:
            script = Common_EventScript_ShowPokemonCenterSign;
            break;
        case 1:
            script = Common_EventScript_ShowPokemartSign;
            break;
        /*case 2:
            script = gUnknown_81A76F0;
            break;
        case 3:
            script = gUnknown_81A76F9;
            break;*/
        case 0xF0:
            script = GetBackgroundEventScriptForSignpost(position);
            if (script == NULL)
                return FALSE;
            break;
        default:
            return FALSE;
    }

    SetupSpecialSignpostScript(script, direction);
    return TRUE;
}

static bool8 GetSpecialSignpostScriptId(u8 metatileBehavior, u8 direction)
{
    if (MetatileBehavior_IsPlayerFacingCommon_EventScript_ShowPokemonCenterSign(metatileBehavior, direction))
        return 0;
    else if (MetatileBehavior_IsPlayerFacingPokeMartSign(metatileBehavior, direction))
        return 1;
    /*else if (MetatileBehavior_IsIndigoPlateauMark(metatileBehavior))
        return 2;
    else if (MetatileBehavior_IsIndigoPlateauMark2(metatileBehavior))
        return 3;*/
    else if (MetatileBehavior_IsSignpost(metatileBehavior))
        return 0xF0;
    return 0xFF;
}

static void SetupSpecialSignpostScript(const u8 *script, u8 direction)
{
    gSpecialVar_Facing = direction;
    ScriptContext1_SetupScript(script);
    EnableExitingFromScriptEarly();
    TextboxUseSignBorder();
}

static const u8 *GetBackgroundEventScriptForSignpost(const struct MapPosition *position)
{
    const u8 *script;
    struct BgEvent *event = GetBackgroundEventAtPosition(&gMapHeader, position->x - 7, position->y - 7, position->height);

    if (event == NULL)
        return NULL;
    
    script = event->bgUnion.script;
    if (script == NULL)
        script = EventScript_TestSignpostMsg;

    return script;
}

static bool8 TryArrowWarp(struct MapPosition *position, u16 metatileBehavior, u8 direction)
{
    u8 delay;
    s8 warpEventId = GetWarpEventAtMapPosition(&gMapHeader, position);

    if (warpEventId != -1)
    {
        if (IsArrowWarpMetatileBehavior(metatileBehavior, direction) == TRUE)
        {
            StoreInitialPlayerAvatarState();
            SetupWarp(&gMapHeader, warpEventId, position);
            DoWarp();
            return TRUE;
        }
        else if (IsStaircaseWarpMetatileBehavior(metatileBehavior, direction) == TRUE)
        {
            delay = 0;
            if (gPlayerAvatar.flags & (PLAYER_AVATAR_FLAG_MACH_BIKE | PLAYER_AVATAR_FLAG_ACRO_BIKE))
            {
                SetPlayerAvatarTransitionFlags(PLAYER_AVATAR_FLAG_ON_FOOT);
                delay = 12;
            }
            StoreInitialPlayerAvatarState();
            SetupWarp(&gMapHeader, warpEventId, position);
            DoStaircaseWarp(metatileBehavior, delay);
            return TRUE;
        }
    }
    return FALSE;
}

static bool8 TryStartWarpEventScript(struct MapPosition *position, u16 metatileBehavior)
{
    s8 warpEventId = GetWarpEventAtMapPosition(&gMapHeader, position);

    if (warpEventId != -1 && IsWarpMetatileBehavior(metatileBehavior) == TRUE)
    {
        StoreInitialPlayerAvatarState();
        SetupWarp(&gMapHeader, warpEventId, position);
        if (MetatileBehavior_IsEscalator(metatileBehavior) == TRUE)
        {
            sub_80AF80C(metatileBehavior);
            return TRUE;
        }
        if (MetatileBehavior_IsLavaridgeB1FWarp(metatileBehavior) == TRUE)
        {
            sub_80AF828();
            return TRUE;
        }
        if (MetatileBehavior_IsLavaridge1FWarp(metatileBehavior) == TRUE)
        {
            sub_80AF838();
            return TRUE;
        }
        if (MetatileBehavior_IsAquaHideoutWarp(metatileBehavior) == TRUE)
        {
            sub_80AF848();
            return TRUE;
        }
        if (MetatileBehavior_IsWarpOrBridge(metatileBehavior) == TRUE)
        {
            sub_80B0268();
            return TRUE;
        }
        if (MetatileBehavior_IsMtPyreHole(metatileBehavior) == TRUE)
        {
            ScriptContext1_SetupScript(gUnknown_082A8350);
            return TRUE;
        }
        if (MetatileBehavior_IsMossdeepGymWarp(metatileBehavior) == TRUE)
        {
            sub_80AF87C();
            return TRUE;
        }
        DoWarp();
        return TRUE;
    }
    return FALSE;
}

static bool8 IsWarpMetatileBehavior(u16 metatileBehavior)
{
    if (MetatileBehavior_IsWarpDoor(metatileBehavior) != TRUE
     && MetatileBehavior_IsLadder(metatileBehavior) != TRUE
     && MetatileBehavior_IsEscalator(metatileBehavior) != TRUE
     && MetatileBehavior_IsNonAnimDoor(metatileBehavior) != TRUE
     && MetatileBehavior_IsLavaridgeB1FWarp(metatileBehavior) != TRUE
     && MetatileBehavior_IsLavaridge1FWarp(metatileBehavior) != TRUE
     && MetatileBehavior_IsAquaHideoutWarp(metatileBehavior) != TRUE
     && MetatileBehavior_IsMtPyreHole(metatileBehavior) != TRUE
     && MetatileBehavior_IsMossdeepGymWarp(metatileBehavior) != TRUE
     && MetatileBehavior_IsWarpOrBridge(metatileBehavior) != TRUE)
        return FALSE;
    return TRUE;
}

bool8 IsStaircaseWarpMetatileBehavior(u16 metatileBehavior, u8 direction)
{
    switch (direction)
    {
    case DIR_WEST:
        if (MetatileBehavior_IsStaircaseUpWest(metatileBehavior) ||
            MetatileBehavior_IsStaircaseDownWest(metatileBehavior))
            return TRUE;
        break;
    case DIR_EAST:
        if (MetatileBehavior_IsStaircaseUpEast(metatileBehavior) ||
            MetatileBehavior_IsStaircaseDownEast(metatileBehavior))
            return TRUE;
        break;
    }
    return FALSE;
}

static bool8 IsArrowWarpMetatileBehavior(u16 metatileBehavior, u8 direction)
{
    switch (direction)
    {
    case DIR_NORTH:
        return MetatileBehavior_IsNorthArrowWarp(metatileBehavior);
    case DIR_SOUTH:
        return MetatileBehavior_IsSouthArrowWarp(metatileBehavior);
    case DIR_WEST:
        return MetatileBehavior_IsWestArrowWarp(metatileBehavior);
    case DIR_EAST:
        return MetatileBehavior_IsEastArrowWarp(metatileBehavior);
    }
    return FALSE;
}

static s8 GetWarpEventAtMapPosition(struct MapHeader *mapHeader, struct MapPosition *position)
{
    return GetWarpEventAtPosition(mapHeader, position->x - 7, position->y - 7, position->height);
}

static void SetupWarp(struct MapHeader *unused, s8 warpEventId, struct MapPosition *position)
{
    const struct WarpEvent *warpEvent;

    u8 trainerHillMapId = GetCurrentTrainerHillMapId();

    if (trainerHillMapId)
    {
        if (trainerHillMapId == sub_81D6490())
        {
            if (warpEventId == 0)
            {
                warpEvent = &gMapHeader.events->warps[0];
            }
            else
            {
                warpEvent = sub_81D6120();
            }
        }
        else if (trainerHillMapId == 5)
        {
            warpEvent = sub_81D6134(warpEventId);
        }
        else
        {
            warpEvent = &gMapHeader.events->warps[warpEventId];
        }
    }
    else
    {
        warpEvent = &gMapHeader.events->warps[warpEventId];
    }

    if (warpEvent->mapNum == MAP_NUM(NONE))
    {
        SetWarpDestinationToDynamicWarp(warpEvent->warpId);
    }
    else
    {
        const struct MapHeader *mapHeader;

        SetWarpDestinationToMapWarp(warpEvent->mapGroup, warpEvent->mapNum, warpEvent->warpId);
        UpdateEscapeWarp(position->x, position->y);
        mapHeader = Overworld_GetMapHeaderByGroupAndId(warpEvent->mapGroup, warpEvent->mapNum);
        if (mapHeader->events->warps[warpEvent->warpId].mapNum == MAP_NUM(NONE))
            SetDynamicWarp(mapHeader->events->warps[warpEventId].warpId, gSaveBlock1Ptr->location.mapGroup, gSaveBlock1Ptr->location.mapNum, warpEventId);
    }
}

static bool8 TryDoorWarp(struct MapPosition *position, u16 metatileBehavior, u8 direction)
{
    s8 warpEventId;

    if (direction == DIR_NORTH)
    {
        if (MetatileBehavior_IsOpenSecretBaseDoor(metatileBehavior) == TRUE)
        {
            WarpIntoSecretBase(position, gMapHeader.events);
            return TRUE;
        }

        if (MetatileBehavior_IsWarpDoor(metatileBehavior) == TRUE)
        {
            warpEventId = GetWarpEventAtMapPosition(&gMapHeader, position);
            if (warpEventId != -1 && IsWarpMetatileBehavior(metatileBehavior) == TRUE)
            {
                StoreInitialPlayerAvatarState();
                SetupWarp(&gMapHeader, warpEventId, position);
                DoDoorWarp();
                return TRUE;
            }
        }
    }
    return FALSE;
}

static s8 GetWarpEventAtPosition(struct MapHeader *mapHeader, u16 x, u16 y, u8 elevation)
{
    s32 i;
    struct WarpEvent *warpEvent = mapHeader->events->warps;
    u8 warpCount = mapHeader->events->warpCount;

    for (i = 0; i < warpCount; i++, warpEvent++)
    {
        if ((u16)warpEvent->x == x && (u16)warpEvent->y == y)
        {
            if (warpEvent->elevation == elevation || warpEvent->elevation == 0)
                return i;
        }
    }
    return -1;
}

static u8 *TryRunCoordEventScript(struct CoordEvent *coordEvent)
{
    if (coordEvent != NULL)
    {
        if (coordEvent->script == NULL)
        {
            DoCoordEventWeather(coordEvent->trigger);
            return NULL;
        }
        if (coordEvent->trigger == 0)
        {
            ScriptContext2_RunNewScript(coordEvent->script);
            return NULL;
        }
        if (VarGet(coordEvent->trigger) == (u8)coordEvent->index)
            return coordEvent->script;
    }
    return NULL;
}

static u8 *GetCoordEventScriptAtPosition(struct MapHeader *mapHeader, u16 x, u16 y, u8 elevation)
{
    s32 i;
    struct CoordEvent *coordEvents = mapHeader->events->coordEvents;
    u8 coordEventCount = mapHeader->events->coordEventCount;

    for (i = 0; i < coordEventCount; i++)
    {
        if ((u16)coordEvents[i].x == x && (u16)coordEvents[i].y == y)
        {
            if (coordEvents[i].elevation == elevation || coordEvents[i].elevation == 0)
            {
                u8 *script = TryRunCoordEventScript(&coordEvents[i]);
                if (script != NULL)
                    return script;
            }
        }
    }
    return NULL;
}

u8 *GetCoordEventScriptAtMapPosition(struct MapPosition *position)
{
    return GetCoordEventScriptAtPosition(&gMapHeader, position->x - 7, position->y - 7, position->height);
}

static struct BgEvent *GetBackgroundEventAtPosition(struct MapHeader *mapHeader, u16 x, u16 y, u8 elevation)
{
    u8 i;
    struct BgEvent *bgEvents = mapHeader->events->bgEvents;
    u8 bgEventCount = mapHeader->events->bgEventCount;

    for (i = 0; i < bgEventCount; i++)
    {
        if ((u16)bgEvents[i].x == x && (u16)bgEvents[i].y == y)
        {
            if (bgEvents[i].elevation == elevation || bgEvents[i].elevation == 0)
                return &bgEvents[i];
        }
    }
    return NULL;
}

bool8 dive_warp(struct MapPosition *position, u16 metatileBehavior)
{
    if (gMapHeader.mapType == MAP_TYPE_UNDERWATER && !MetatileBehavior_IsUnableToEmerge(metatileBehavior))
    {
        if (SetDiveWarpEmerge(position->x - 7, position->y - 7))
        {
            StoreInitialPlayerAvatarState();
            DoDiveWarp();
            PlaySE(SE_W291);
            return TRUE;
        }
    }
    else if (MetatileBehavior_IsDiveable(metatileBehavior) == TRUE)
    {
        if (SetDiveWarpDive(position->x - 7, position->y - 7))
        {
            StoreInitialPlayerAvatarState();
            DoDiveWarp();
            PlaySE(SE_W291);
            return TRUE;
        }
    }
    return FALSE;
}

u8 TrySetDiveWarp(void)
{
    s16 x, y;
    u8 metatileBehavior;

    PlayerGetDestCoords(&x, &y);
    metatileBehavior = MapGridGetMetatileBehaviorAt(x, y);
    if (gMapHeader.mapType == MAP_TYPE_UNDERWATER && !MetatileBehavior_IsUnableToEmerge(metatileBehavior))
    {
        if (SetDiveWarpEmerge(x - 7, y - 7) == TRUE)
            return 1;
    }
    else if (MetatileBehavior_IsDiveable(metatileBehavior) == TRUE)
    {
        if (SetDiveWarpDive(x - 7, y - 7) == TRUE)
            return 2;
    }
    return 0;
}

const u8 *GetEventObjectScriptPointerPlayerFacing(void)
{
    u8 direction;
    struct MapPosition position;

    direction = GetPlayerMovementDirection();
    GetInFrontOfPlayerPosition(&position);
    return GetInteractedEventObjectScript(&position, MapGridGetMetatileBehaviorAt(position.x, position.y), direction);
}

int SetCableClubWarp(void)
{
    struct MapPosition position;

    GetPlayerMovementDirection();  //unnecessary
    GetPlayerPosition(&position);
    MapGridGetMetatileBehaviorAt(position.x, position.y);  //unnecessary
    SetupWarp(&gMapHeader, GetWarpEventAtMapPosition(&gMapHeader, &position), &position);
    return 0;
}
