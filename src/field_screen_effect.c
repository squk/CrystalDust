#include "global.h"
#include "cable_club.h"
#include "event_data.h"
#include "fieldmap.h"
#include "field_camera.h"
#include "field_door.h"
#include "field_effect.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "field_screen_effect.h"
#include "field_special_scene.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "link.h"
#include "link_rfu.h"
#include "load_save.h"
#include "main.h"
#include "menu.h"
#include "mirage_tower.h"
#include "event_obj_lock.h"
#include "metatile_behavior.h"
#include "palette.h"
#include "overworld.h"
#include "scanline_effect.h"
#include "script.h"
#include "sound.h"
#include "start_menu.h"
#include "task.h"
#include "text.h"
#include "constants/event_object_movement_constants.h"
#include "constants/songs.h"
#include "constants/rgb.h"
#include "trainer_hill.h"
#include "event_obj_lock.h"
#include "fldeff.h"

extern const u16 gUnknown_82EC7CC[];

// This file's functions.
static void sub_8080B9C(u8);
static void task_map_chg_seq_0807E20C(u8);
static void task_map_chg_seq_0807E2CC(u8);
static void Task_StaircaseWarpIn(u8);
static void task0A_fade_n_map_maybe(u8);
static void sub_808115C(u8);
static void palette_bg_faded_fill_white(void);
static void sub_80AF438(u8);
static bool32 WaitForWeatherFadeIn(void);
static void task0A_mpl_807E31C(u8 taskId);
static void Task_StaircaseWarpOut(u8 taskId);
static void sub_80AFA0C(u8 taskId);
static void sub_80AFA88(u8 taskId);
static void task50_0807F0C8(u8);
static void AnimatePlayerWalkOutOnStaircase(s16 a0, s16 a1, s16 *a2, s16 *a3, s16 *a4);
static void BeginAnimatingPlayerWalkOutOnStaircase(s16 behavior, s16 *a1, s16 *a2);
static void SetStaircaseTargetPosValues(u8 behavior, s16 *a1, s16 *a2);
static bool8 AnimatePlayerWalkInOnStaircase(s16 *a0, s16 *a1, s16 *a2, s16 *a3, s16 *a4);
static void BeginAnimatingPlayerWalkInOnStaircase(s16 *a0, s16 *a1, s16 *a2, s16 *a3, s16 *a4);
static void sub_807F204(u8 taskId);
static void sub_807F0EC();
static void sub_807F114();
static void sub_807F2FC(u8 taskId);
static void sub_807DF4C(bool8 a0);

// const
const u16 sFlashLevelPixelRadii[] = { 200, 72, 64, 56, 48, 40, 32, 24, 0 };
const s32 gMaxFlashLevel = 8;

const struct ScanlineEffectParams sFlashEffectParams =
{
    (void *)REG_ADDR_WIN0H,
    ((DMA_ENABLE | DMA_START_HBLANK | DMA_REPEAT | DMA_DEST_RELOAD) << 16) | 1,
    1
};

// code
static void palette_bg_faded_fill_white(void)
{
    CpuFastFill16(RGB_WHITE, gPlttBufferFaded, PLTT_SIZE);
}

static void palette_bg_faded_fill_black(void)
{
    CpuFastFill16(RGB_BLACK, gPlttBufferFaded, PLTT_SIZE);
}

void pal_fill_for_maplights(void)
{
    u8 previousMapType = GetLastUsedWarpMapType();
    switch (GetMapPairFadeFromType(previousMapType, GetCurrentMapType()))
    {
    case 0:
        palette_bg_faded_fill_black();
        FadeScreen(FADE_FROM_BLACK, 0);
        palette_bg_faded_fill_black();
        break;
    case 1:
        palette_bg_faded_fill_white();
        FadeScreen(FADE_FROM_WHITE, 0);
        palette_bg_faded_fill_white();
    }
}

static void sub_80AF08C(void)
{
    u8 previousMapType = GetLastUsedWarpMapType();
    switch (GetMapPairFadeFromType(previousMapType, GetCurrentMapType()))
    {
    case 0:
        palette_bg_faded_fill_black();
        FadeScreen(FADE_FROM_BLACK, 3);
        palette_bg_faded_fill_black();
        break;
    case 1:
        palette_bg_faded_fill_white();
        FadeScreen(FADE_FROM_WHITE, 3);
        palette_bg_faded_fill_white();
    }
}

void pal_fill_black(void)
{
    palette_bg_faded_fill_black();
    FadeScreen(FADE_FROM_BLACK, 0);
    palette_bg_faded_fill_black();
}

void WarpFadeScreen(void)
{
    u8 currentMapType = GetCurrentMapType();
    switch (GetMapPairFadeToType(currentMapType, GetDestinationWarpMapHeader()->mapType))
    {
    case 0:
        FadeScreen(FADE_TO_BLACK, 0);
        break;
    case 1:
        FadeScreen(FADE_TO_WHITE, 0);
    }
}

static void sub_80AF0F4(u8 arg)
{
    sub_808C0A8(!arg);
}

static void task0A_nop_for_a_while(u8 taskId)
{
    if (WaitForWeatherFadeIn() == TRUE)
        DestroyTask(taskId);
}

void sub_80AF128(void)
{
    ScriptContext2_Enable();
    Overworld_PlaySpecialMapMusic();
    pal_fill_black();
    CreateTask(task0A_nop_for_a_while, 10);
}

static void task0A_asap_script_env_2_enable_and_set_ctx_running(u8 taskID)
{
    if (WaitForWeatherFadeIn() == TRUE)
    {
        DestroyTask(taskID);
        EnableBothScriptContexts();
    }
}

void FieldCallback_ReturnToEventScript2(void)
{
    ScriptContext2_Enable();
    Overworld_PlaySpecialMapMusic();
    pal_fill_black();
    CreateTask(task0A_asap_script_env_2_enable_and_set_ctx_running, 10);
}

void sub_80AF188(void)
{
    ScriptContext2_Enable();
    pal_fill_black();
    CreateTask(task0A_asap_script_env_2_enable_and_set_ctx_running, 10);
}

static void task_mpl_807DD60(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->data[0])
    {
    case 0:
        task->data[1] = sub_80B3050();
        task->data[0]++;
        break;
    case 1:
        if (gTasks[task->data[1]].isActive != TRUE)
        {
            pal_fill_for_maplights();
            task->data[0]++;
        }
        break;
    case 2:
        if (WaitForWeatherFadeIn() == TRUE)
        {
            ScriptContext2_Disable();
            DestroyTask(taskId);
        }
        break;
    }
}

void sub_80AF214(void)
{
    ScriptContext2_Enable();
    Overworld_PlaySpecialMapMusic();
    palette_bg_faded_fill_black();
    CreateTask(task_mpl_807DD60, 10);
}

static void sub_80AF234(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->data[0])
    {
    case 0:
        sub_800ADF8();
        task->data[0]++;
        break;
    case 1:
        if (!IsLinkTaskFinished())
        {
            if (++task->data[1] > 1800)
                sub_8011170(0x6000);
        }
        else
        {
            pal_fill_for_maplights();
            task->data[0]++;
        }
        break;
    case 2:
        if (WaitForWeatherFadeIn() == TRUE)
        {
            sub_8009F18();
            ScriptContext2_Disable();
            DestroyTask(taskId);
        }
        break;
    }
}

void sub_80AF2B4(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->data[0])
    {
    case 0:
        sub_800ADF8();
        task->data[0]++;
        break;
    case 1:
        if (IsLinkTaskFinished())
        {
            task->data[0]++;
        }
        break;
    case 2:
        sub_8009F18();
        ResetAllMultiplayerState();
        ScriptContext2_Disable();
        DestroyTask(taskId);
        break;
    }
}

void sub_80AF314(void)
{
    ScriptContext2_Enable();
    Overworld_PlaySpecialMapMusic();
    palette_bg_faded_fill_black();
    CreateTask(sub_80AF234, 10);
}

static void sub_80AF334(bool8 forceBlack)
{
    s16 x, y;
    u8 behavior;
    TaskFunc func;

    PlayerGetDestCoords(&x, &y);
    behavior = MapGridGetMetatileBehaviorAt(x, y);

    if (MetatileBehavior_IsDoor(behavior) == TRUE)
    {
        func = sub_80AF438;
    }
    else
    {
        sub_807DF4C(forceBlack);
        if (MetatileBehavior_IsNonAnimDoor(behavior) == TRUE)
            func = task_map_chg_seq_0807E20C;
        else if (MetatileBehavior_IsStaircase(behavior) == TRUE)
            func = Task_StaircaseWarpIn;
        else
            func = task_map_chg_seq_0807E2CC;
    }
    CreateTask(func, 10);
}

static void sub_807DF4C(bool8 forceBlack)
{
    if (forceBlack)
        pal_fill_black();
    else
        pal_fill_for_maplights();
}

void mapldr_default(void)
{
    Overworld_PlaySpecialMapMusic();
    //pal_fill_for_maplights();
    sub_80AF334(FALSE);
    ScriptContext2_Enable();
}

void sub_80AF3B0(void)
{
    Overworld_PlaySpecialMapMusic();
    sub_80AF08C();
    sub_80AF334(FALSE);
    ScriptContext2_Enable();
}

void sub_80AF3C8(void)
{
    if (!sub_81D6534())
        Overworld_PlaySpecialMapMusic();
    pal_fill_black();
    sub_80AF334(FALSE);
    ScriptContext2_Enable();
}

void sub_80AF3E8(void)
{
    Overworld_PlaySpecialMapMusic();
    pal_fill_for_maplights();
    PlaySE(SE_TK_WARPOUT);
    CreateTask(task0A_mpl_807E31C, 10);
    ScriptContext2_Enable();
}

void sub_80AF40C(void)
{
    Overworld_PlaySpecialMapMusic();
    pal_fill_for_maplights();
    PlaySE(SE_TK_WARPOUT);
    CreateTask(task_map_chg_seq_0807E2CC, 10);
    ScriptContext2_Enable();
    sub_8085540(0xE);
}

static void sub_80AF438(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    s16 *x = &task->data[2];
    s16 *y = &task->data[3];

    switch (task->data[0])
    {
    /*case 0:
        sub_80AF0F4(0);
        FreezeEventObjects();
        PlayerGetDestCoords(x, y);
        FieldSetDoorOpened(*x, *y);
        task->data[0] = 1;
        break;*/
    case 0:
        sub_80AF0F4(0);
        FreezeEventObjects();
        sub_807F114();
        sub_80AF08C();
        task->data[0] = 1;
        break;
    case 1:
        if (++task->data[15] == 25)
        {
            PlayerGetDestCoords(x, y);
            PlaySE(GetDoorSoundEffect(*x, *y));
            FieldAnimateDoorOpen(*x, *y);
            task->data[0] = 2;
        }
        break;
    case 2:
        if (!FieldIsDoorAnimationRunning())
        {
            u8 eventObjId;
            PlayerGetDestCoords(x, y);
            sub_80AF0F4(1);
            eventObjId = GetEventObjectIdByLocalIdAndMap(0xFF, 0, 0);
            EventObjectSetHeldMovement(&gEventObjects[eventObjId], MOVEMENT_ACTION_WALK_NORMAL_DOWN);
            task->data[0] = 3;
        }
        break;
    case 3:
        if (++task->data[14] == 14)
        {
            FieldAnimateDoorClose(*x, *y);
            task->data[0] = 4;
        }
        break;
    case 4:
        if (WaitForWeatherFadeIn() && walkrun_is_standing_still() && !FieldIsDoorAnimationRunning() && !FuncIsActiveTask(sub_807F204))
        {
            u8 eventObjId = GetEventObjectIdByLocalIdAndMap(0xFF, 0, 0);
            EventObjectClearHeldMovementIfFinished(&gEventObjects[eventObjId]);
            task->data[0] = 5;
        }
        break;
    /*case 1:
        if (WaitForWeatherFadeIn())
        {
            u8 eventObjId;
            sub_80AF0F4(1);
            eventObjId = GetEventObjectIdByLocalIdAndMap(0xFF, 0, 0);
            EventObjectSetHeldMovement(&gEventObjects[eventObjId], MOVEMENT_ACTION_WALK_NORMAL_DOWN);
            task->data[0] = 2;
        }
        break;
    case 2:
        if (walkrun_is_standing_still())
        {
            u8 eventObjId;
            task->data[1] = FieldAnimateDoorClose(*x, *y);
            eventObjId = GetEventObjectIdByLocalIdAndMap(0xFF, 0, 0);
            EventObjectClearHeldMovementIfFinished(&gEventObjects[eventObjId]);
            task->data[0] = 3;
        }
        break;
    case 3:
        if (task->data[1] < 0 || gTasks[task->data[1]].isActive != TRUE)
            task->data[0] = 4;
        break;*/
    case 5:
        UnfreezeEventObjects();
        ScriptContext2_Disable();
        DestroyTask(taskId);
        break;
    }
}

static void task_map_chg_seq_0807E20C(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    s16 *x = &task->data[2];
    s16 *y = &task->data[3];

    switch (task->data[0])
    {
    case 0:
        sub_80AF0F4(0);
        FreezeEventObjects();
        PlayerGetDestCoords(x, y);
        task->data[0] = 1;
        break;
    case 1:
        if (WaitForWeatherFadeIn())
        {
            u8 eventObjId;
            sub_80AF0F4(1);
            eventObjId = GetEventObjectIdByLocalIdAndMap(0xFF, 0, 0);
            EventObjectSetHeldMovement(&gEventObjects[eventObjId], GetWalkNormalMovementAction(GetPlayerFacingDirection()));
            task->data[0] = 2;
        }
        break;
    case 2:
        if (walkrun_is_standing_still())
        {
            UnfreezeEventObjects();
            task->data[0] = 3;
        }
        break;
    case 3:
        ScriptContext2_Disable();
        DestroyTask(taskId);
        break;
    }
}

static void task_map_chg_seq_0807E2CC(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        FreezeEventObjects();
        ScriptContext2_Enable();
        gTasks[taskId].data[0]++;
        break;
    case 1:
        if (WaitForWeatherFadeIn())
        {
            UnfreezeEventObjects();
            ScriptContext2_Disable();
            DestroyTask(taskId);
        }
        break;
    }
}

static void sub_80AF660(u8 taskId)
{
    if (WaitForWeatherFadeIn() == TRUE)
    {
        DestroyTask(taskId);
        CreateTask(sub_809FA34, 80);
    }
}

void sub_80AF688(void)
{
    pal_fill_black();
    CreateTask(sub_80AF660, 0x50);
    ScriptContext2_Enable();
}

bool8 sub_80AF6A4(void)
{
    sub_809FA18();
    return FALSE;
}

static void task_mpl_807E3C8(u8 taskId)
{
    if (WaitForWeatherFadeIn() == 1)
    {
        ScriptContext2_Disable();
        DestroyTask(taskId);
        ScriptUnfreezeEventObjects();
    }
}

void sub_80AF6D4(void)
{
    ScriptContext2_Enable();
    pal_fill_black();
    CreateTask(task_mpl_807E3C8, 10);
}

void sub_80AF6F0(void)
{
    ScriptContext2_Enable();
    Overworld_PlaySpecialMapMusic();
    pal_fill_black();
    CreateTask(task_mpl_807E3C8, 10);
}

static bool32 PaletteFadeActive(void)
{
    return gPaletteFade.active;
}

static bool32 WaitForWeatherFadeIn(void)
{
    if (IsWeatherNotFadingIn() == TRUE)
        return TRUE;
    else
        return FALSE;
}

void DoWarp(void)
{
    ScriptContext2_Enable();
    TryFadeOutOldMapMusic();
    WarpFadeScreen();
    PlayRainStoppingSoundEffect();
    PlaySE(SE_KAIDAN);
    gFieldCallback = mapldr_default;
    CreateTask(sub_80AFA0C, 10);
}

void DoDiveWarp(void)
{
    ScriptContext2_Enable();
    TryFadeOutOldMapMusic();
    WarpFadeScreen();
    PlayRainStoppingSoundEffect();
    gFieldCallback = mapldr_default;
    CreateTask(sub_80AFA0C, 10);
}

void DoStaircaseWarp(u16 behavior, u16 unk)
{
    u8 taskId = CreateTask(Task_StaircaseWarpOut, 10);
    gTasks[taskId].data[1] = behavior;
    gTasks[taskId].data[15] = unk;
    Task_StaircaseWarpOut(taskId);
}

void sub_80AF79C(void)
{
    ScriptContext2_Enable();
    TryFadeOutOldMapMusic();
    FadeScreen(FADE_TO_WHITE, 8);
    PlayRainStoppingSoundEffect();
    gFieldCallback = sub_80AF3B0;
    CreateTask(sub_80AFA0C, 10);
}

void DoDoorWarp(void)
{
    ScriptContext2_Enable();
    gFieldCallback = mapldr_default;
    CreateTask(sub_80AFA88, 10);
}

void DoFallWarp(void)
{
    DoDiveWarp();
    gFieldCallback = sub_80B6B68;
}

void sub_80AF80C(u8 metatileBehavior)
{
    ScriptContext2_Enable();
    sub_80B6E4C(metatileBehavior, 10);
}

void sub_80AF828(void)
{
    ScriptContext2_Enable();
    sub_80B75D8(10);
}

void sub_80AF838(void)
{
    ScriptContext2_Enable();
    sub_80B7A74(10);
}

void sub_80AF848(void)
{
    ScriptContext2_Enable();
    TryFadeOutOldMapMusic();
    WarpFadeScreen();
    PlaySE(SE_TK_WARPIN);
    CreateTask(sub_80AFA0C, 10);
    gFieldCallback = sub_80AF3E8;
}

void sub_80AF87C(void)
{
    sub_8085540(1);
    ScriptContext2_Enable();
    SaveEventObjects();
    TryFadeOutOldMapMusic();
    WarpFadeScreen();
    PlaySE(SE_TK_WARPIN);
    CreateTask(sub_80AFA0C, 10);
    gFieldCallback = sub_80AF40C;
}

void sub_80AF8B8(void)
{
    ScriptContext2_Enable();
    WarpFadeScreen();
    CreateTask(sub_80AFA0C, 10);
    gFieldCallback = sub_80FB768;
}

static void sub_80AF8E0(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->data[0])
    {
    case 0:
        ScriptContext2_Enable();
        task->data[0]++;
        break;
    case 1:
        if (!PaletteFadeActive() && BGMusicStopped())
            task->data[0]++;
        break;
    case 2:
        WarpIntoMap();
        SetMainCallback2(sub_8086074);
        DestroyTask(taskId);
        break;
    }
}

void sub_80AF948(void)
{
    ScriptContext2_Enable();
    TryFadeOutOldMapMusic();
    WarpFadeScreen();
    PlaySE(SE_KAIDAN);
    CreateTask(sub_80AF8E0, 10);
}

static void Task_ReturnToWorldFromLinkRoom(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        ClearLinkCallback_2();
        FadeScreen(FADE_TO_BLACK, 0);
        TryFadeOutOldMapMusic();
        PlaySE(SE_KAIDAN);
        data[0]++;
        break;
    case 1:
        if (!PaletteFadeActive() && BGMusicStopped())
        {
            sub_800AC34();
            data[0]++;
        }
        break;
    case 2:
        if (!gReceivedRemoteLinkPlayers)
        {
            WarpIntoMap();
            SetMainCallback2(CB2_LoadMap);
            DestroyTask(taskId);
        }
        break;
    }
}

void sub_80AF9F8(void)
{
    CreateTask(Task_ReturnToWorldFromLinkRoom, 10);
}

static void sub_80AFA0C(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->data[0])
    {
    case 0:
        FreezeEventObjects();
        ScriptContext2_Enable();
        task->data[0]++;
        break;
    case 1:
        if (!PaletteFadeActive())
        {
            if (task->data[1] == 0)
            {
                ClearMirageTowerPulseBlendEffect();
                task->data[1] = 1;
            }
            if (BGMusicStopped())
                task->data[0]++;
        }
        break;
    case 2:
        WarpIntoMap();
        SetMainCallback2(CB2_LoadMap);
        DestroyTask(taskId);
        break;
    }
}

static void sub_80AFA88(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    s16 *x = &task->data[2];
    s16 *y = &task->data[3];

    switch (task->data[0])
    {
    case 0:
        FreezeEventObjects();
        PlayerGetDestCoords(x, y);
        PlaySE(GetDoorSoundEffect(*x, *y - 1));
        task->data[1] = FieldAnimateDoorOpen(*x, *y - 1);
        task->data[0] = 1;
        break;
    case 1:
        if (task->data[1] < 0 || gTasks[task->data[1]].isActive != TRUE)
        {
            u8 eventObjId;
            eventObjId = GetEventObjectIdByLocalIdAndMap(0xFF, 0, 0);
            EventObjectClearHeldMovementIfActive(&gEventObjects[eventObjId]);
            eventObjId = GetEventObjectIdByLocalIdAndMap(0xFF, 0, 0);
            EventObjectSetHeldMovement(&gEventObjects[eventObjId], MOVEMENT_ACTION_WALK_NORMAL_UP);
            task->data[0] = 2;
        }
        break;
    case 2:
        if (walkrun_is_standing_still())
        {
            u8 eventObjId;
            task->data[1] = FieldAnimateDoorClose(*x, *y - 1);
            eventObjId = GetEventObjectIdByLocalIdAndMap(0xFF, 0, 0);
            EventObjectClearHeldMovementIfFinished(&gEventObjects[eventObjId]);
            sub_80AF0F4(0);
            task->data[0] = 3;
        }
        break;
    case 3:
        if (task->data[1] < 0 || gTasks[task->data[1]].isActive != TRUE)
        {
            task->data[0] = 4;
        }
        break;
    case 4:
        TryFadeOutOldMapMusic();
        WarpFadeScreen();
        PlayRainStoppingSoundEffect();
        task->data[0] = 0;
        task->func = sub_80AFA0C;
        break;
    }
}

static void task0A_fade_n_map_maybe(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->data[0])
    {
    case 0:
        FreezeEventObjects();
        ScriptContext2_Enable();
        task->data[0]++;
        break;
    case 1:
        if (!PaletteFadeActive() && BGMusicStopped())
        {
            task->data[0]++;
        }
        break;
    case 2:
        WarpIntoMap();
        SetMainCallback2(sub_8086024);
        DestroyTask(taskId);
        break;
    }
}

void sub_80AFC60(void)
{
    ScriptContext2_Enable();
    TryFadeOutOldMapMusic();
    WarpFadeScreen();
    PlayRainStoppingSoundEffect();
    PlaySE(SE_KAIDAN);
    gFieldCallback = sub_80AF3C8;
    CreateTask(task0A_fade_n_map_maybe, 10);
}

static void SetFlashScanlineEffectWindowBoundary(u16 *dest, u32 y, s32 left, s32 right)
{
    if (y <= 160)
    {
        if (left < 0)
            left = 0;
        if (left > 255)
            left = 255;
        if (right < 0)
            right = 0;
        if (right > 255)
            right = 255;
        dest[y] = (left << 8) | right;
    }
}

static void SetFlashScanlineEffectWindowBoundaries(u16 *dest, s32 centerX, s32 centerY, s32 radius)
{
    s32 r = radius;
    s32 v2 = radius;
    s32 v3 = 0;
    while (r >= v3)
    {
        SetFlashScanlineEffectWindowBoundary(dest, centerY - v3, centerX - r, centerX + r);
        SetFlashScanlineEffectWindowBoundary(dest, centerY + v3, centerX - r, centerX + r);
        SetFlashScanlineEffectWindowBoundary(dest, centerY - r, centerX - v3, centerX + v3);
        SetFlashScanlineEffectWindowBoundary(dest, centerY + r, centerX - v3, centerX + v3);
        v2 -= (v3 * 2) - 1;
        v3++;
        if (v2 < 0)
        {
            v2 += 2 * (r - 1);
            r--;
        }
    }
}

static void SetFlash2ScanlineEffectWindowBoundary(u16 *dest, u32 y, s32 left, s32 right)
{
    if (y <= 160)
    {
        if (left < 0)
            left = 0;
        if (left > 240)
            left = 240;
        if (right < 0)
            right = 0;
        if (right > 240)
            right = 240;
        dest[y] = (left << 8) | right;
    }
}

static void SetFlash2ScanlineEffectWindowBoundaries(u16 *dest, s32 centerX, s32 centerY, s32 radius)
{
    s32 r = radius;
    s32 v2 = radius;
    s32 v3 = 0;
    while (r >= v3)
    {
        SetFlash2ScanlineEffectWindowBoundary(dest, centerY - v3, centerX - r, centerX + r);
        SetFlash2ScanlineEffectWindowBoundary(dest, centerY + v3, centerX - r, centerX + r);
        SetFlash2ScanlineEffectWindowBoundary(dest, centerY - r, centerX - v3, centerX + v3);
        SetFlash2ScanlineEffectWindowBoundary(dest, centerY + r, centerX - v3, centerX + v3);
        v2 -= (v3 * 2) - 1;
        v3++;
        if (v2 < 0)
        {
            v2 += 2 * (r - 1);
            r--;
        }
    }
}

#define tFlashCenterX        data[1]
#define tFlashCenterY        data[2]
#define tCurFlashRadius      data[3]
#define tDestFlashRadius     data[4]
#define tFlashRadiusDelta    data[5]
#define tClearScanlineEffect data[6]

static void UpdateFlashLevelEffect(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        SetFlashScanlineEffectWindowBoundaries(gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer], tFlashCenterX, tFlashCenterY, tCurFlashRadius);
        data[0] = 1;
        break;
    case 1:
        SetFlashScanlineEffectWindowBoundaries(gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer], tFlashCenterX, tFlashCenterY, tCurFlashRadius);
        data[0] = 0;
        tCurFlashRadius += tFlashRadiusDelta;
        if (tCurFlashRadius > tDestFlashRadius)
        {
            if (tClearScanlineEffect == 1)
            {
                ScanlineEffect_Stop();
                data[0] = 2;
            }
            else
            {
                DestroyTask(taskId);
            }
        }
        break;
    case 2:
        ScanlineEffect_Clear();
        DestroyTask(taskId);
        break;
    }
}

static void UpdateFlash2LevelEffect(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        SetFlash2ScanlineEffectWindowBoundaries(gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer], tFlashCenterX, tFlashCenterY, tCurFlashRadius);
        data[0] = 1;
        break;
    case 1:
        SetFlash2ScanlineEffectWindowBoundaries(gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer], tFlashCenterX, tFlashCenterY, tCurFlashRadius);
        data[0] = 0;
        tCurFlashRadius += tFlashRadiusDelta;
        if (tCurFlashRadius > tDestFlashRadius)
        {
            if (tClearScanlineEffect == 1)
            {
                ScanlineEffect_Stop();
                data[0] = 2;
            }
            else
            {
                DestroyTask(taskId);
            }
        }
        break;
    case 2:
        ScanlineEffect_Clear();
        DestroyTask(taskId);
        break;
    }
}

static void sub_80AFF90(u8 taskId)
{
    if (!FuncIsActiveTask(UpdateFlashLevelEffect))
    {
        EnableBothScriptContexts();
        DestroyTask(taskId);
    }
}

static void sub_80AFFB8(void)
{
    if (!FuncIsActiveTask(sub_80AFF90))
        CreateTask(sub_80AFF90, 80);
}

static u8 sub_80AFFDC(s32 centerX, s32 centerY, s32 initialFlashRadius, s32 destFlashRadius, s32 clearScanlineEffect, u8 delta)
{
    u8 taskId = CreateTask(UpdateFlashLevelEffect, 80);
    s16 *data = gTasks[taskId].data;

    tCurFlashRadius = initialFlashRadius;
    tDestFlashRadius = destFlashRadius;
    tFlashCenterX = centerX;
    tFlashCenterY = centerY;
    tClearScanlineEffect = clearScanlineEffect;

    if (initialFlashRadius < destFlashRadius)
        tFlashRadiusDelta = delta;
    else
        tFlashRadiusDelta = -delta;

    return taskId;
}

static u8 sub_80B003C(s32 centerX, s32 centerY, s32 initialFlashRadius, s32 destFlashRadius, s32 clearScanlineEffect, u8 delta)
{
    u8 taskId = CreateTask(UpdateFlash2LevelEffect, 80);
    s16 *data = gTasks[taskId].data;

    tCurFlashRadius = initialFlashRadius;
    tDestFlashRadius = destFlashRadius;
    tFlashCenterX = centerX;
    tFlashCenterY = centerY;
    tClearScanlineEffect = clearScanlineEffect;

    if (initialFlashRadius < destFlashRadius)
        tFlashRadiusDelta = delta;
    else
        tFlashRadiusDelta = -delta;

    return taskId;
}

#undef tCurFlashRadius
#undef tDestFlashRadius
#undef tFlashRadiusDelta
#undef tClearScanlineEffect

void sub_80B009C(u8 flashLevel)
{
    u8 curFlashLevel = Overworld_GetFlashLevel();
    u8 value = 0;
    if (!flashLevel)
        value = 1;
    sub_80AFFDC(120, 80, sFlashLevelPixelRadii[curFlashLevel], sFlashLevelPixelRadii[flashLevel], value, 1);
    sub_80AFFB8();
    ScriptContext2_Enable();
}

void WriteFlashScanlineEffectBuffer(u8 flashLevel)
{
    if (flashLevel)
    {
        SetFlashScanlineEffectWindowBoundaries(&gScanlineEffectRegBuffers[0][0], 120, 80, sFlashLevelPixelRadii[flashLevel]);
        CpuFastSet(&gScanlineEffectRegBuffers[0], &gScanlineEffectRegBuffers[1], 480);
    }
}

void WriteBattlePyramidViewScanlineEffectBuffer(void)
{
    SetFlashScanlineEffectWindowBoundaries(&gScanlineEffectRegBuffers[0][0], 120, 80, gSaveBlock2Ptr->frontier.pyramidLightRadius);
    CpuFastSet(&gScanlineEffectRegBuffers[0], &gScanlineEffectRegBuffers[1], 480);
}

static void task0A_mpl_807E31C(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        FreezeEventObjects();
        ScriptContext2_Enable();
        sub_808D194();
        gTasks[taskId].data[0]++;
        break;
    case 1:
        if (WaitForWeatherFadeIn() && sub_808D1B4() != TRUE)
        {
            UnfreezeEventObjects();
            ScriptContext2_Disable();
            DestroyTask(taskId);
        }
        break;
    }
}

static void sub_80B01BC(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    switch (task->data[0])
    {
    case 0:
        FreezeEventObjects();
        ScriptContext2_Enable();
        PlaySE(SE_TK_WARPIN);
        sub_808D1C8();
        task->data[0]++;
        break;
    case 1:
        if (!sub_808D1E8())
        {
            WarpFadeScreen();
            task->data[0]++;
        }
        break;
    case 2:
        if (!PaletteFadeActive() && BGMusicStopped())
            task->data[0]++;
        break;
    case 3:
        WarpIntoMap();
        SetMainCallback2(CB2_LoadMap);
        DestroyTask(taskId);
        break;
    }
}

void sub_80B0244(void)
{
    ScriptContext2_Enable();
    CreateTask(sub_80AFA0C, 10);
    gFieldCallback = sub_80AF3E8;
}

void sub_80B0268(void)
{
    ScriptContext2_Enable();
    gFieldCallback = mapldr_default;
    CreateTask(sub_80B01BC, 10);
}

static void sub_80B028C(u8 a1)
{
    int i;
    u16 color[1];

    if (!a1)
        color[0] = RGB_RED;
    else
        color[0] = RGB_BLUE;

    for (i = 0; i < 16; i++)
    {
        LoadPalette(color, 0xF0 + i, 2);
    }
}

static bool8 sub_80B02C8(u16 a1)
{
    u8 lo = REG_BLDALPHA & 0xFF;
    u8 hi = REG_BLDALPHA >> 8;

    if (a1)
    {
        if (lo)
        {
            lo--;
        }
    }
    else
    {
        if (hi < 0x10)
        {
            hi++;
        }
    }

    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(lo, hi));

    if (lo == 0 && hi == 0x10)
        return TRUE;
    else
        return FALSE;
}

static void sub_80B0318(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        data[6] = REG_DISPCNT;
        data[7] = REG_BLDCNT;
        data[8] = REG_BLDALPHA;
        data[9] = REG_WININ;
        data[10] = REG_WINOUT;
        ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN1_ON);
        SetGpuRegBits(REG_OFFSET_BLDCNT, gUnknown_82EC7CC[0]);
        SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(12, 7));
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG_ALL | WININ_WIN0_OBJ | WININ_WIN0_CLR);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG1 | WINOUT_WIN01_BG2 | WINOUT_WIN01_BG3 | WINOUT_WIN01_OBJ);
        sub_8199C30(0, 0, 0, 0x1E, 0x14, 0xF);
        schedule_bg_copy_tilemap_to_vram(0);
        SetFlash2ScanlineEffectWindowBoundaries(&gScanlineEffectRegBuffers[0][0], data[2], data[3], 1);
        CpuFastSet(&gScanlineEffectRegBuffers[0], &gScanlineEffectRegBuffers[1], 480);
        ScanlineEffect_SetParams(sFlashEffectParams);
        data[0] = 1;
        break;
    case 1:
        sub_8199DF0(0, PIXEL_FILL(1), 0, 1);
        sub_80B028C(data[1]);
        sub_80B003C(data[2], data[3], 1, 160, 1, 2);
        data[0] = 2;
        break;
    case 2:
        if (!FuncIsActiveTask(UpdateFlash2LevelEffect))
        {
            EnableBothScriptContexts();
            data[0] = 3;
        }
        break;
    case 3:
        InstallCameraPanAheadCallback();
        SetCameraPanningCallback(NULL);
        data[5] = 0;
        data[4] = 4;
        data[0] = 4;
        break;
    case 4:
        data[4]--;
        if (!data[4])
        {
            s32 panning;
            data[4] = 4;
            data[5] ^= 1;
            if (data[5])
                panning = 4;
            else
                panning = -4;
            SetCameraPanning(0, panning);
        }
        break;
    case 6:
        InstallCameraPanAheadCallback();
        data[4] = 8;
        data[0] = 7;
        break;
    case 7:
        data[4]--;
        if (!data[4])
        {
            data[4] = 8;
            data[5] ^= 1;
            if (sub_80B02C8(data[5]) == TRUE)
            {
                data[0] = 5;
                sub_8199DF0(0, PIXEL_FILL(0), 0, 1);
            }
        }
        break;
    case 5:
        SetGpuReg(REG_OFFSET_WIN0H, 255);
        SetGpuReg(REG_OFFSET_DISPCNT, data[6]);
        SetGpuReg(REG_OFFSET_BLDCNT, data[7]);
        SetGpuReg(REG_OFFSET_BLDALPHA, data[8]);
        SetGpuReg(REG_OFFSET_WININ, data[9]);
        SetGpuReg(REG_OFFSET_WINOUT, data[10]);
        EnableBothScriptContexts();
        DestroyTask(taskId);
        break;
    }
}

void sub_80B0534(void)
{
    u8 taskId = CreateTask(sub_80B0318, 80);
    s16 *data = gTasks[taskId].data;

    if (gSpecialVar_Result == 0)
    {
        data[1] = 0;
        data[2] = 104;
    }
    else if (gSpecialVar_Result == 1)
    {
        data[1] = 1;
        data[2] = 136;
    }
    else if (gSpecialVar_Result == 2)
    {
        data[1] = 0;
        data[2] = 120;
    }
    else
    {
        data[1] = 1;
        data[2] = 120;
    }

    data[3] = 80;
}

void sub_80B058C(void)
{
    u8 taskId = FindTaskIdByFunc(sub_80B0318);
    gTasks[taskId].data[0] = 6;
}

void sub_80B05B4(void)
{
    Overworld_FadeOutMapMusic();
    CreateTask(task50_0807F0C8, 80);
}

static void task50_0807F0C8(u8 taskId)
{
    if (BGMusicStopped() == TRUE)
    {
        DestroyTask(taskId);
        EnableBothScriptContexts();
    }
}

static void sub_807F0EC()
{
    u8 taskId = CreateTask(sub_807F204, 80);
    gTasks[taskId].data[10] = 0;
}

static void sub_807F114()
{
    u8 taskId = CreateTask(sub_807F204, 80);
    gTasks[taskId].data[10] = 1;
}

static void sub_807F13C(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    data[0] = GetGpuReg(REG_OFFSET_DISPCNT);
    data[1] = GetGpuReg(REG_OFFSET_WININ);
    data[2] = GetGpuReg(REG_OFFSET_WINOUT);
    data[3] = GetGpuReg(REG_OFFSET_BLDCNT);
    data[4] = GetGpuReg(REG_OFFSET_BLDALPHA);
    data[5] = GetGpuReg(REG_OFFSET_WIN0H);
    data[6] = GetGpuReg(REG_OFFSET_WIN0V);
    data[7] = GetGpuReg(REG_OFFSET_WIN1H);
    data[8] = GetGpuReg(REG_OFFSET_WIN1V);
}

static void sub_807F1A0(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    SetGpuReg(REG_OFFSET_DISPCNT, data[0]);
    SetGpuReg(REG_OFFSET_WININ, data[1]);
    SetGpuReg(REG_OFFSET_WINOUT, data[2]);
    SetGpuReg(REG_OFFSET_BLDCNT, data[3]);
    SetGpuReg(REG_OFFSET_BLDALPHA, data[4]);
    SetGpuReg(REG_OFFSET_WIN0H, data[5]);
    SetGpuReg(REG_OFFSET_WIN0V, data[6]);
    SetGpuReg(REG_OFFSET_WIN1H, data[7]);
    SetGpuReg(REG_OFFSET_WIN1V, data[8]);
}

static void sub_807F204(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[9])
    {
        case 0:
            sub_807F13C(taskId);
            SetGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON);
            if (data[10] == 0)
            {
                SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(0, 0));
                SetGpuReg(REG_OFFSET_WIN1H, WIN_RANGE(240, 255));
                SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(0, 255));
                SetGpuReg(REG_OFFSET_WIN1V, WIN_RANGE(0, 255));
            }
            else
            {
                SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(0, 120));
                SetGpuReg(REG_OFFSET_WIN1H, WIN_RANGE(120, 255));
                SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(0, 255));
                SetGpuReg(REG_OFFSET_WIN1V, WIN_RANGE(0, 255));
            }
            SetGpuReg(REG_OFFSET_WININ, 0);
            SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL |
                                         WINOUT_WIN01_OBJ |
                                         WINOUT_WIN01_CLR);
            data[9] = 1;
            break;
        case 1:
            CreateTask(sub_807F2FC, 80);
            data[9] = 2;
            break;
        case 2:
            if (!FuncIsActiveTask(sub_807F2FC))
                data[9] = 3;
            break;
        case 3:
            sub_807F1A0(taskId);
            DestroyTask(taskId);
            break;
    }
}

static void sub_807F2FC(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    s16 *data2 = gTasks[FindTaskIdByFunc(sub_807F204)].data;
    s16 val;
    s16 val2;

    if (data2[10] == 0)
    {
        val = data[0];
        val2 = 240 - data[0];

        if (val > 120)
        {
            DestroyTask(taskId);
            return;
        }
    }
    else
    {
        val = 120 - data[0];
        val2 = 120 + data[0];

        if (val < 0)
        {
            DestroyTask(taskId);
            return;
        }
    }

    SetGpuReg(REG_OFFSET_WIN0H, val);
    SetGpuReg(REG_OFFSET_WIN1H, WIN_RANGE(val2, 240));

    if (val < 90)
        data[0] += 4;
    else
        data[0] += 2;
}

static void Task_StaircaseWarpOut(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    struct EventObject *eventObj = &gEventObjects[gPlayerAvatar.eventObjectId];
    struct Sprite *sprite = &gSprites[gPlayerAvatar.spriteId];

    switch (data[0])
    {
    case 0:
        ScriptContext2_Enable();
        FreezeEventObjects();
        CameraObjectReset2();
        data[0]++;
        break;
    case 1:
        if (!EventObjectIsMovementOverridden(eventObj) || EventObjectClearHeldMovementIfFinished(eventObj))
        {
            if (data[15] != 0)
            {
                data[15]--;
            }
            else
            {
                TryFadeOutOldMapMusic();
                PlayRainStoppingSoundEffect();
                sprite->oam.priority = 1;
                BeginAnimatingPlayerWalkOutOnStaircase(data[1], &data[2], &data[3]);
                PlaySE(SE_KAIDAN);
                data[0]++;
            }
        }
        break;
    case 2:
        AnimatePlayerWalkOutOnStaircase(data[2], data[3], &data[4], &data[5], &data[6]);
        if (++data[15] > 11)
        {
            WarpFadeScreen();
            data[0]++;
        }
        break;
    case 3:
        AnimatePlayerWalkOutOnStaircase(data[2], data[3], &data[4], &data[5], &data[6]);
        if (!PaletteFadeActive() && BGMusicStopped())
            data[0]++;
        break;
    default:
        gFieldCallback = mapldr_default;
        WarpIntoMap();
        SetMainCallback2(CB2_LoadMap);
        DestroyTask(taskId);
        break;
    }
}

static void AnimatePlayerWalkOutOnStaircase(s16 xDelta, s16 yDelta, s16 *x, s16 *y, s16 *frame)
{
    struct EventObject *eventObj = &gEventObjects[gPlayerAvatar.eventObjectId];
    struct Sprite *sprite = &gSprites[gPlayerAvatar.spriteId];

    if (yDelta > 0 || *frame > 6)
        *y += yDelta;

    *x += xDelta;
    (*frame)++;

    gSprites[gPlayerAvatar.spriteId].pos2.x = *x / 32;
    gSprites[gPlayerAvatar.spriteId].pos2.y = *y / 32;

    if (eventObj->heldMovementFinished)
        EventObjectForceSetHeldMovement(eventObj, GetWalkInPlaceNormalMovementAction(GetPlayerFacingDirection()));
}

static void BeginAnimatingPlayerWalkOutOnStaircase(s16 behavior, s16 *x, s16 *y)
{
    struct EventObject *eventObj = &gEventObjects[gPlayerAvatar.eventObjectId];

    EventObjectForceSetHeldMovement(eventObj, GetWalkInPlaceNormalMovementAction(GetPlayerFacingDirection()));
    SetStaircaseTargetPosValues(behavior, x, y);
}

static void SetStaircaseTargetPosValues(u8 behavior, s16 *x, s16 *y)
{
    if (MetatileBehavior_IsStaircaseUpEast(behavior))
    {
        *x = 16;
        *y = -10;
    }
    else if (MetatileBehavior_IsStaircaseUpWest(behavior))
    {
        *x = -17;
        *y = -10;
    }
    else if (MetatileBehavior_IsStaircaseDownEast(behavior))
    {
        *x = 17;
        *y = 3;
    }
    else if (MetatileBehavior_IsStaircaseDownWest(behavior))
    {
        *x = -17;
        *y = 3;
    }
    else
    {
        *x = 0;
        *y = 0;
    }
}

static void Task_StaircaseWarpIn(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
        default:
            if (WaitForWeatherFadeIn() == TRUE)
            {
                CameraObjectReset1();
                ScriptContext2_Disable();
                DestroyTask(taskId);
            }
            break;
        case 0:
            Overworld_PlaySpecialMapMusic();
            pal_fill_for_maplights();
            ScriptContext2_Enable();
            BeginAnimatingPlayerWalkInOnStaircase(&data[1], &data[2], &data[3], &data[4], &data[5]);
            data[0]++;
            break;
        case 1:
            if (!AnimatePlayerWalkInOnStaircase(&data[1], &data[2], &data[3], &data[4], &data[5]))
                data[0]++;
            break;
    }
}

static void BeginAnimatingPlayerWalkInOnStaircase(s16 *xPixels, s16 *yPixels, s16 *xSubpixels, s16 *ySubpixels, s16 *frame)
{
    s16 x, y;
    u8 behavior;
    u8 direction;

    PlayerGetDestCoords(&x, &y);
    behavior = MapGridGetMetatileBehaviorAt(x, y);

    if (MetatileBehavior_IsStaircaseDownEast(behavior) || MetatileBehavior_IsStaircaseUpEast(behavior))
        direction = DIR_WEST;
    else
        direction = DIR_EAST;

    EventObjectForceSetHeldMovement(&gEventObjects[gPlayerAvatar.eventObjectId], GetWalkInPlaceFastMovementAction(direction));
    SetStaircaseTargetPosValues(behavior, xPixels, yPixels);

    *xSubpixels = *xPixels * 16;
    *ySubpixels = *yPixels * 16;
    *frame = 16;
    gSprites[gPlayerAvatar.spriteId].pos2.x = *xSubpixels / 32;
    gSprites[gPlayerAvatar.spriteId].pos2.y = *ySubpixels / 32;
    *xPixels = -*xPixels;
    *yPixels = -*yPixels;
}

static bool8 AnimatePlayerWalkInOnStaircase(s16 *xDelta, s16 *yDelta, s16 *x, s16 *y, s16 *frame)
{
    struct Sprite *playerSprite = &gSprites[gPlayerAvatar.spriteId];

    if (*frame == 0)
    {
        playerSprite->pos2.x = 0;
        playerSprite->pos2.y = 0;
        return FALSE;
    }
    else
    {
        *x += *xDelta;
        *y += *yDelta;
        playerSprite->pos2.x = *x / 32;
        playerSprite->pos2.y = *y / 32;
        (*frame)--;
        return TRUE;
    }
}
