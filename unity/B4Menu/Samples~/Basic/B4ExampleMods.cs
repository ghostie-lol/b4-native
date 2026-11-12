using B4;
using UnityEngine;

/// <summary>Minimal example: register entries from your own scripts. Ids are yours to choose (unique per entry).</summary>
public class B4ExampleMods : MonoBehaviour
{
    const int ExampleToggleId = 100;
    const int ExampleActionId = 200;
    const int ExampleSoundId = 300;

    void Start()
    {
        var menu = B4MenuHost.Instance;
        if (menu == null || !menu.IsReady) return;

        menu.RegisterToggle(ExampleToggleId, "Example toggle", B4Category.Game, false,
            on => Debug.Log("Example toggle: " + on));

        menu.RegisterAction(ExampleActionId, "Example action", B4Category.Movement,
            () => Debug.Log("Example action fired"));

        menu.RegisterAction(ExampleSoundId, "Example sound", B4Category.Sound,
            () => Debug.Log("Play a clip here"));

        menu.VisibilityChanged += v => Debug.Log("Menu visible: " + v);
    }
}
