Scriptname SCIE_ToggleContainerScript extends ActiveMagicEffect
{Toggles the crafting source status on the container in the player's crosshairs.
 Cycles through 3 states: OFF -> LOCAL -> GLOBAL -> OFF
 Uses native functions to manage container states with the DLL.
 Attach this script to the SCIE_ToggleEffect magic effect.

 HOW IT WORKS:
 1. Player aims at a container and activates the power
 2. Script gets the crosshair reference via Game.GetCurrentCrosshairRef()
 3. Calls SCIE_NativeFunctions.ToggleCraftingContainer() which cycles the state
 4. Provides multi-modal feedback: sound, rumble, notification, shader

 PROPERTIES (set in xEdit on the magic effect):
   EnableSound   - Sound to play when enabling (e.g., UIItemGenericUp)
   DisableSound  - Sound to play when disabling (e.g., UIItemGenericDown)
   EnableShader  - EFSH for enable glow (green - local state)
   GlobalShader  - EFSH for global glow (gold/blue - global state)
   DisableShader - EFSH for disable glow (red - off state)

 NOTE: Shader duration is passed directly to Play() - 1.5 seconds for brief feedback.}

; =============================================================================
; PROPERTIES - Set these in xEdit on the magic effect
; =============================================================================

; Sound to play when enabling a container (recommended: UIItemGenericUp)
Sound Property EnableSound Auto

; Sound to play when disabling a container (recommended: UIItemGenericDown)
Sound Property DisableSound Auto

; Shader for local enable feedback (green glow)
EffectShader Property EnableShader Auto

; Shader for global promotion feedback (gold/blue glow)
EffectShader Property GlobalShader Auto

; Shader for disable feedback (red glow)
EffectShader Property DisableShader Auto

Event OnEffectStart(Actor akTarget, Actor akCaster)
    ; Check if the SKSE plugin is loaded
    if !SCIE_NativeFunctions.IsPluginLoaded()
        Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_ErrPluginNotLoaded"))
        Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_ErrCheckInstall"))
        return
    endif

    ; Get what the player is looking at (SKSE function)
    ObjectReference target = Game.GetCurrentCrosshairRef()

    ; Validate target
    if target == None
        Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_ErrNoTarget"))
        return
    endif

    ; Check if target is a container or player-owned mount
    if !SCIE_NativeFunctions.IsValidToggleTarget(target)
        Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_ErrNotContainer"))
        return
    endif

    ; Block toggling ON for unsafe containers if setting is disabled
    ; Still allow cycling (including OFF) for already-marked containers
    if SCIE_NativeFunctions.IsContainerUnsafe(target)
        if !SCIE_NativeFunctions.GetAllowUnsafeContainers()
            int currentState = SCIE_NativeFunctions.GetContainerState(target)
            if currentState == 0
                Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_ErrUnsafeContainer"))
                return
            endif
        endif
    endif

    ; Get container name with warning suffixes
    string containerName = SCIE_NativeFunctions.GetContainerDisplayName(target)

    ; Capture current state before toggling (for non-persistent skip detection)
    int previousState = SCIE_NativeFunctions.GetContainerState(target)

    ; Get player for sound playback and animation
    Actor player = Game.GetPlayer()

    ; Trigger activation animation (may be picked up by animation mods)
    Debug.SendAnimationEvent(player, "ActivateStart")

    ; Cycle the container state: OFF -> LOCAL -> GLOBAL -> OFF
    ; Returns: 0=off, 1=local, 2=global
    ; Note: Non-persistent containers skip GLOBAL (LOCAL -> OFF) in the DLL
    int newState = SCIE_NativeFunctions.ToggleCraftingContainer(target)

    ; Detect non-persistent GLOBAL skip: was LOCAL (1), expected GLOBAL (2), got OFF (0)
    bool skippedGlobal = (previousState == 1 && newState == 0)

    if newState == 0
        ; Container is now OFF
        if DisableSound
            DisableSound.Play(player)
        endif
        Game.ShakeController(0.4, 0.1, 0.08)

        if skippedGlobal
            ; Non-persistent container — GLOBAL was skipped, explain why
            Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_ErrNonPersistent"))
        endif
        Debug.Notification(SCIE_NativeFunctions.TranslateFormat("$SCIE_NotifyStateOff", containerName))

        if DisableShader
            DisableShader.Play(target, 1.5)
        endif

    elseif newState == 1
        ; Container is now LOCAL
        if EnableSound
            EnableSound.Play(player)
        endif
        Game.ShakeController(0.3, 0.3, 0.2)
        Debug.Notification(SCIE_NativeFunctions.TranslateFormat("$SCIE_NotifyStateOn", containerName))

        if EnableShader
            EnableShader.Play(target, 1.5)
        endif

    elseif newState == 2
        ; Container is now GLOBAL
        if EnableSound
            EnableSound.Play(player)
        endif
        Game.ShakeController(0.3, 0.3, 0.2)
        Debug.Notification(SCIE_NativeFunctions.TranslateFormat("$SCIE_NotifyStateGlobal", containerName))

        if GlobalShader
            GlobalShader.Play(target, 1.5)
        elseif EnableShader
            ; Fallback to enable shader if global shader not set
            EnableShader.Play(target, 1.5)
        endif
    endif
EndEvent
