#pragma once

#include "../dsp/Block.h"

struct ModuleInfo
{
    juce::String typeId;
    juce::String displayName;
    ModuleCategory category;
};

class ModuleFactory
{
public:
    static juce::Array<ModuleInfo> getModulesForCategory (ModuleCategory category);
    static juce::Array<ModuleInfo> getAllLoadableModules();

    static BlockPtr create (const juce::String& typeId);
    static juce::String displayNameForType (const juce::String& typeId);
};
