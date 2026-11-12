using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEngine;

namespace B4.Editor
{
    /// <summary>Configures libb4menu.so as an Android/ARM64-only native plugin when it is imported.</summary>
    internal sealed class B4PluginPostprocessor : AssetPostprocessor
    {
        void OnPreprocessAsset()
        {
            if (!assetPath.EndsWith("libb4menu.so")) return;
            if (!(assetImporter is PluginImporter imp)) return;
            imp.SetCompatibleWithAnyPlatform(false);
            imp.SetCompatibleWithEditor(false);
            imp.SetCompatibleWithPlatform(BuildTarget.Android, true);
            imp.SetPlatformData(BuildTarget.Android, "CPU", "ARM64");
        }
    }

    /// <summary>Warns at build time if the Android player can't load an arm64-only library.</summary>
    internal sealed class B4BuildCheck : IPreprocessBuildWithReport
    {
        public int callbackOrder => 0;

        public void OnPreprocessBuild(BuildReport report)
        {
            if (report.summary.platform != BuildTarget.Android) return;
            if ((PlayerSettings.Android.targetArchitectures & AndroidArchitecture.ARM64) == 0)
                Debug.LogWarning("[B4] libb4menu.so is arm64-v8a only, but ARM64 is not enabled in Player Settings > Android > Target Architectures.");
            if (PlayerSettings.GetScriptingBackend(NamedBuildTarget.Android) != ScriptingImplementation.IL2CPP)
                Debug.LogWarning("[B4] ARM64 Android players require the IL2CPP scripting backend.");
        }
    }
}
