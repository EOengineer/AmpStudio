#pragma once

#include "Block.h"
#include "BypassBlock.h"

/**
 * Fixed-length processing chain with realtime-safe reorder.
 * Slot contents are Block instances; empty slots are BypassBlock.
 */
class Chain
{
public:
    static constexpr int numSlots = 6;

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void chainChanged() = 0;
    };

    Chain();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    int getNumSlots() const noexcept { return numSlots; }

    Block* getBlock (int index) noexcept;
    const Block* getBlock (int index) const noexcept;

    /** Replace a slot (message thread). Takes ownership. */
    void setBlock (int index, BlockPtr block);

    void clearBlock (int index);

    /** Move slot `from` to index `to` (message thread). Params travel with the block. */
    void moveBlock (int from, int to);

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree,
                        std::function<BlockPtr (const juce::String& typeId)> factory);

private:
    void notifyListeners();
    void ensurePrepared (Block& block);

    std::array<BlockPtr, numSlots> slots;
    juce::dsp::ProcessSpec currentSpec {};
    bool isPrepared = false;

    juce::ListenerList<Listener> listeners;
    juce::SpinLock processLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Chain)
};
