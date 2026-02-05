Scriptname SCIE_DetectContainersScript extends ActiveMagicEffect
{Fire-and-forget spell that highlights nearby crafting containers.
 Green glow = local, gold = global, red = explicitly disabled by player.
 Attach this script to the SCIE_DetectEffect magic effect.

 HOW DURATION WORKS:
 - Magic effect duration (set in xEdit) controls how long highlighting lasts
 - OnEffectFinish calls Stop() on all shaders when the effect expires
 - Default effect duration: 15 seconds

 PROPERTIES (set in xEdit):
   EnabledShader  - EFSH for local containers (green glow)
   GlobalShader   - EFSH for global containers (gold glow)
   DisabledShader - EFSH for disabled containers (red glow)
   MaxDistance    - Search radius in game units (0 = use INI default)}

; Shader properties - fill in xEdit with EFSH records
EffectShader Property EnabledShader Auto
EffectShader Property GlobalShader Auto
EffectShader Property DisabledShader Auto

; Max distance to search (0 = use INI default)
float Property MaxDistance = 0.0 Auto

; Track containers we applied shaders to (for cleanup in OnEffectFinish)
ObjectReference[] enabledContainers
ObjectReference[] globalContainers
ObjectReference[] disabledContainers

Event OnEffectStart(Actor akTarget, Actor akCaster)
    ; Check if the SKSE plugin is loaded
    if !SCIE_NativeFunctions.IsPluginLoaded()
        Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_ErrPluginNotLoaded"))
        Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_ErrCheckInstall"))
        return
    endif

    ; Get all containers in range by state
    enabledContainers = SCIE_NativeFunctions.GetEnabledContainersInCell(MaxDistance)
    globalContainers = SCIE_NativeFunctions.GetGlobalContainersInCell(MaxDistance)
    disabledContainers = SCIE_NativeFunctions.GetDisabledContainersInCell(MaxDistance)

    ; Apply shaders to enabled containers - separate local from global
    int localCount = 0
    int containerState = 0
    ObjectReference cont

    if EnabledShader
        int i = 0
        while i < enabledContainers.Length
            cont = enabledContainers[i]
            if cont
                containerState = SCIE_NativeFunctions.GetContainerState(cont)
                if containerState == 1
                    ; Local container - green shader
                    EnabledShader.Play(cont, 999.0)
                    localCount += 1
                endif
            endif
            i += 1
        endwhile
    endif

    ; Apply global shader (gold)
    if GlobalShader
        int i = 0
        while i < globalContainers.Length
            cont = globalContainers[i]
            if cont
                GlobalShader.Play(cont, 999.0)
            endif
            i += 1
        endwhile
    elseif EnabledShader
        ; Fallback: use enabled shader for globals too
        int i = 0
        while i < globalContainers.Length
            cont = globalContainers[i]
            if cont
                EnabledShader.Play(cont, 999.0)
            endif
            i += 1
        endwhile
    endif

    ; Apply disabled shader (red)
    if DisabledShader
        int i = 0
        while i < disabledContainers.Length
            cont = disabledContainers[i]
            if cont
                DisabledShader.Play(cont, 999.0)
            endif
            i += 1
        endwhile
    endif

    ; Show count notification
    int globalCount = globalContainers.Length
    int disabledCount = disabledContainers.Length

    if localCount > 0 || globalCount > 0 || disabledCount > 0
        Debug.Notification(SCIE_NativeFunctions.TranslateFormat("$SCIE_DetectCounts", localCount as string, globalCount as string, disabledCount as string))
    else
        Debug.Notification(SCIE_NativeFunctions.Translate("$SCIE_DetectNone"))
    endif
EndEvent

Event OnEffectFinish(Actor akTarget, Actor akCaster)
    ; Stop shaders on all tracked containers
    ObjectReference ref
    int idx

    if EnabledShader
        idx = 0
        while idx < enabledContainers.Length
            ref = enabledContainers[idx]
            if ref
                EnabledShader.Stop(ref)
            endif
            idx += 1
        endwhile
    endif

    if GlobalShader
        idx = 0
        while idx < globalContainers.Length
            ref = globalContainers[idx]
            if ref
                GlobalShader.Stop(ref)
            endif
            idx += 1
        endwhile
    elseif EnabledShader
        ; Was using fallback - stop enabled shader on globals
        idx = 0
        while idx < globalContainers.Length
            ref = globalContainers[idx]
            if ref
                EnabledShader.Stop(ref)
            endif
            idx += 1
        endwhile
    endif

    if DisabledShader
        idx = 0
        while idx < disabledContainers.Length
            ref = disabledContainers[idx]
            if ref
                DisabledShader.Stop(ref)
            endif
            idx += 1
        endwhile
    endif
EndEvent
