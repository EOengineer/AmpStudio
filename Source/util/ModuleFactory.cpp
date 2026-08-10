#include "ModuleFactory.h"
#include "../dsp/BypassBlock.h"
#include "../dsp/fx/TubeScreamer.h"
#include "../dsp/amps/Champ5F1.h"
#include "../dsp/cabs/CabIR.h"
#include "../dsp/captures/NeuralCapture.h"
#include "ParamIDs.h"

juce::Array<ModuleInfo> ModuleFactory::getModulesForCategory (ModuleCategory category)
{
    juce::Array<ModuleInfo> result;

    for (const auto& info : getAllLoadableModules())
        if (info.category == category)
            result.add (info);

    return result;
}

juce::Array<ModuleInfo> ModuleFactory::getAllLoadableModules()
{
    juce::Array<ModuleInfo> modules;
    modules.add ({ ModuleIds::tubeScreamer,  "Tube Screamer",  ModuleCategory::fx });
    modules.add ({ ModuleIds::champ5F1,      "Champ 5F1",      ModuleCategory::amp });
    modules.add ({ ModuleIds::cabIR,         "Cab IR",         ModuleCategory::cab });
    modules.add ({ ModuleIds::neuralCapture, "Neural Capture", ModuleCategory::capture });
    return modules;
}

BlockPtr ModuleFactory::create (const juce::String& typeId)
{
    if (typeId == ModuleIds::tubeScreamer)
        return std::make_unique<TubeScreamer>();

    if (typeId == ModuleIds::champ5F1)
        return std::make_unique<Champ5F1>();

    if (typeId == ModuleIds::cabIR)
        return std::make_unique<CabIR>();

    if (typeId == ModuleIds::neuralCapture)
        return std::make_unique<NeuralCapture>();

    return std::make_unique<BypassBlock>();
}

juce::String ModuleFactory::displayNameForType (const juce::String& typeId)
{
    for (const auto& info : getAllLoadableModules())
        if (info.typeId == typeId)
            return info.displayName;

    if (typeId == ModuleIds::bypass)
        return "Empty";

    return typeId;
}
