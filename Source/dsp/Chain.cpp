#include "Chain.h"

Chain::Chain()
{
    for (auto& slot : slots)
        slot = std::make_unique<BypassBlock>();
}

void Chain::prepare (const juce::dsp::ProcessSpec& spec)
{
    const juce::SpinLock::ScopedLockType lock (processLock);
    currentSpec = spec;
    isPrepared = true;

    for (auto& slot : slots)
        if (slot != nullptr)
            slot->prepare (spec);
}

void Chain::reset()
{
    const juce::SpinLock::ScopedLockType lock (processLock);

    for (auto& slot : slots)
        if (slot != nullptr)
            slot->reset();
}

void Chain::process (juce::AudioBuffer<float>& buffer)
{
    const juce::SpinLock::ScopedLockType lock (processLock);

    for (auto& slot : slots)
        if (slot != nullptr)
            slot->process (buffer);
}

Block* Chain::getBlock (int index) noexcept
{
    if (! juce::isPositiveAndBelow (index, numSlots))
        return nullptr;

    return slots[(size_t) index].get();
}

const Block* Chain::getBlock (int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, numSlots))
        return nullptr;

    return slots[(size_t) index].get();
}

void Chain::ensurePrepared (Block& block)
{
    if (isPrepared)
        block.prepare (currentSpec);
}

void Chain::setBlock (int index, BlockPtr block)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! juce::isPositiveAndBelow (index, numSlots) || block == nullptr)
        return;

    ensurePrepared (*block);

    {
        const juce::SpinLock::ScopedLockType lock (processLock);
        slots[(size_t) index] = std::move (block);
    }

    notifyListeners();
}

void Chain::clearBlock (int index)
{
    setBlock (index, std::make_unique<BypassBlock>());
}

void Chain::moveBlock (int from, int to)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (from == to
        || ! juce::isPositiveAndBelow (from, numSlots)
        || ! juce::isPositiveAndBelow (to, numSlots))
        return;

    {
        const juce::SpinLock::ScopedLockType lock (processLock);

        auto moving = std::move (slots[(size_t) from]);

        if (from < to)
        {
            for (int i = from; i < to; ++i)
                slots[(size_t) i] = std::move (slots[(size_t) i + 1]);
        }
        else
        {
            for (int i = from; i > to; --i)
                slots[(size_t) i] = std::move (slots[(size_t) i - 1]);
        }

        slots[(size_t) to] = std::move (moving);
    }

    notifyListeners();
}

void Chain::addListener (Listener* listener)
{
    listeners.add (listener);
}

void Chain::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

void Chain::notifyListeners()
{
    listeners.call ([] (Listener& l) { l.chainChanged(); });
}

juce::ValueTree Chain::toValueTree() const
{
    juce::ValueTree tree ("Chain");

    for (int i = 0; i < numSlots; ++i)
    {
        juce::ValueTree slotTree ("Slot");
        slotTree.setProperty ("index", i, nullptr);

        if (auto* block = slots[(size_t) i].get())
        {
            slotTree.setProperty ("typeId", block->getTypeId(), nullptr);
            slotTree.appendChild (block->getState().createCopy(), nullptr);
        }
        else
        {
            slotTree.setProperty ("typeId", ModuleIds::bypass, nullptr);
        }

        tree.appendChild (slotTree, nullptr);
    }

    return tree;
}

void Chain::fromValueTree (const juce::ValueTree& tree,
                           std::function<BlockPtr (const juce::String& typeId)> factory)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! tree.hasType ("Chain") || factory == nullptr)
        return;

    std::array<BlockPtr, numSlots> loaded;

    for (int i = 0; i < numSlots; ++i)
        loaded[(size_t) i] = std::make_unique<BypassBlock>();

    for (const auto& child : tree)
    {
        if (! child.hasType ("Slot"))
            continue;

        const int index = (int) child.getProperty ("index", -1);

        if (! juce::isPositiveAndBelow (index, numSlots))
            continue;

        const auto typeId = child.getProperty ("typeId").toString();
        auto block = factory (typeId);

        if (block == nullptr)
            block = std::make_unique<BypassBlock>();

        if (child.getNumChildren() > 0)
            block->getState().copyPropertiesAndChildrenFrom (child.getChild (0), nullptr);

        ensurePrepared (*block);
        loaded[(size_t) index] = std::move (block);
    }

    {
        const juce::SpinLock::ScopedLockType lock (processLock);
        slots = std::move (loaded);
    }

    notifyListeners();
}
