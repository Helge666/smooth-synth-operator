namespace sso
{

SynthMemoryPanel::SynthMemoryPanel (PresetLibrary& lib, SynthController& c)
    : library (lib), controller (c), reader (c)
{
    cache.resize ((size_t) controller.getDevice().getNumSlots());
    loadCache();

    readButton.setTooltip ("Read all sounds stored in the synth (takes about 20 seconds)");
    readButton.onClick = [this] { readAll(); };
    importButton.setTooltip ("Copy this bank's sounds into the library");
    importButton.onClick = [this] { importBank(); };
    addAndMakeVisible (readButton);
    addAndMakeVisible (importButton);

    setTooltip ("Click: load into editor. Drop a sound from the list to store it in the synth.");
}

SynthMemoryPanel::~SynthMemoryPanel()
{
    reader.cancel();
}

int SynthMemoryPanel::numBanks() const
{
    return controller.getDevice().getNumSlots() / slotsPerBank();
}

juce::Rectangle<int> SynthMemoryPanel::bankButtonBounds (int b) const
{
    auto r = getLocalBounds().reduced (12, 0).withTrimmedTop (headerHeight).withHeight (bankRowHeight);
    auto w = (float) r.getWidth() / (float) numBanks();
    return juce::Rectangle<float> ((float) r.getX() + (float) b * w, (float) r.getY(), w, (float) r.getHeight())
               .reduced (1.0f, 0.0f).toNearestInt();
}

juce::Rectangle<int> SynthMemoryPanel::tileBounds (int i) const
{
    auto r = getLocalBounds().reduced (12, 0).withTrimmedTop (headerHeight + bankRowHeight + 6);
    auto w = (float) r.getWidth() / 4.0f;
    return juce::Rectangle<float> ((float) r.getX() + (float) (i % 4) * w, (float) r.getY() + (float) (i / 4) * (tileHeight + 4),
                                   w, (float) tileHeight)
               .reduced (2.0f, 0.0f).toNearestInt();
}

int SynthMemoryPanel::slotAt (juce::Point<int> p) const
{
    for (int i = 0; i < slotsPerBank(); ++i)
        if (tileBounds (i).contains (p))
            return bank * slotsPerBank() + i;

    return -1;
}

void SynthMemoryPanel::paint (juce::Graphics& g)
{
    paintSectionFrame (g, getLocalBounds(), "Synth memory", theme::orange);
    auto& device = controller.getDevice();

    for (int b = 0; b < numBanks(); ++b)
    {
        auto r = bankButtonBounds (b);
        g.setColour (b == bank ? theme::orange : theme::control);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (b == bank ? theme::background : theme::text);
        g.setFont (theme::label (13.0f, true));
        g.drawText (device.getSlotName (b * slotsPerBank()).upToFirstOccurrenceOf ("-", false, false), r, juce::Justification::centred);
    }

    for (int i = 0; i < slotsPerBank(); ++i)
    {
        auto slot = bank * slotsPerBank() + i;
        auto r = tileBounds (i);
        auto& patch = cache[(size_t) slot];
        auto hover = slot == hoverSlot;

        g.setColour (hover ? theme::yellow : theme::control);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);

        auto inner = r.reduced (6, 4);
        g.setColour (hover ? theme::background : theme::textDim);
        g.setFont (theme::label (12.0f, true));
        g.drawText (device.getSlotName (slot), inner.removeFromTop (14), juce::Justification::centredLeft);

        auto name = patch.has_value() ? device.getPatchName (*patch) : juce::String ("?");
        g.setColour (hover ? theme::background : (patch.has_value() ? theme::text : theme::textOff));
        g.setFont (theme::label (13.0f));
        g.drawFittedText (name.isEmpty() ? juce::String ("untitled") : name, inner, juce::Justification::centredLeft, 1, 0.8f);
    }

    g.setColour (theme::textDim);
    g.setFont (theme::label (12.0f));
    auto statusArea = getLocalBounds().reduced (12, 0).removeFromBottom (62).removeFromTop (20);
    g.drawFittedText (status, statusArea, juce::Justification::centredLeft, 1, 0.8f);
}

void SynthMemoryPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 0).removeFromBottom (26 + 8).withTrimmedBottom (8);
    readButton.setBounds (r.removeFromLeft (r.getWidth() / 2).withTrimmedRight (3));
    importButton.setBounds (r.withTrimmedLeft (3));
}

void SynthMemoryPanel::setStatus (const juce::String& s)
{
    status = s;
    repaint();
}

void SynthMemoryPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int b = 0; b < numBanks(); ++b)
    {
        if (bankButtonBounds (b).contains (e.getPosition()))
        {
            bank = b;
            repaint();
            return;
        }
    }

    auto slot = slotAt (e.getPosition());

    if (slot < 0)
        return;

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem ("Load into editor", [this, slot] { loadSlotIntoEditor (slot); });
        menu.addItem ("Write current editor sound here...", [this, slot]
        {
            Preset p;
            p.name = controller.getPatchName();
            p.patch = controller.getCurrentPatch();
            writePresetToSlot (p, slot);
        });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
        return;
    }

    loadSlotIntoEditor (slot);
}

void SynthMemoryPanel::loadSlotIntoEditor (int slot)
{
    if (reader.isRunning())
        return;

    auto slotName = controller.getDevice().getSlotName (slot);
    setStatus ("Loading " + slotName + "...");
    controller.silence();

    reader.start ({ slot }, nullptr, [this, slot, slotName] (BankReader::Result result, bool complete)
    {
        if (! complete || result.empty())
        {
            setStatus ("No answer from the synth for " + slotName);
            return;
        }

        cache[(size_t) slot] = result.front().patch;
        saveCache();
        controller.loadPatch (result.front().patch, true);
        controller.setPresetRef ({});
        setStatus ("Loaded " + slotName + " into the editor");
    }, false);
}

void SynthMemoryPanel::writePresetToSlot (const Preset& preset, int slot)
{
    auto slotName = controller.getDevice().getSlotName (slot);
    auto existing = cache[(size_t) slot].has_value() ? controller.getDevice().getPatchName (*cache[(size_t) slot]) : juce::String();

    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::WarningIcon)
                       .withTitle ("Store in synth memory")
                       .withMessage ("Write \"" + preset.name + "\" to " + slotName + "?\n\n"
                                     + (existing.isNotEmpty() ? "This replaces \"" + existing + "\". " : juce::String())
                                     + "The synth's memory protect switch must be off.")
                       .withButton ("Write")
                       .withButton ("Cancel")
                       .withAssociatedComponent (this);

    messageBox = juce::NativeMessageBox::showScopedAsync (options, [this, preset, slot, slotName] (int result)
    {
        if (result != 1)    // first button = 1, cancel/close = 0
            return;

        controller.writePatchToSlot (preset.patch, slot);
        setStatus ("Writing " + slotName + "...");

        juce::Timer::callAfterDelay (400, [weak = juce::Component::SafePointer<SynthMemoryPanel> (this), preset, slot, slotName]
        {
            if (weak == nullptr)
                return;

            weak->reader.start ({ slot }, nullptr, [weak, preset, slot, slotName] (BankReader::Result r, bool complete)
            {
                if (weak == nullptr)
                    return;

                if (complete && ! r.empty())
                    weak->cache[(size_t) slot] = r.front().patch;

                weak->saveCache();

                if (complete && ! r.empty() && r.front().patch == preset.patch)
                    weak->setStatus ("Stored \"" + preset.name + "\" in " + slotName);
                else if (complete)
                    weak->setStatus (slotName + " unchanged - is memory protect switched off?");
                else
                    weak->setStatus ("Could not verify " + slotName + " (no answer)");
            });
        });
    });
}

void SynthMemoryPanel::readAll()
{
    if (reader.isRunning())
    {
        reader.cancel();
        return;
    }

    std::vector<int> slots;
    for (int s = 0; s < controller.getDevice().getNumSlots(); ++s)
        slots.push_back (s);

    readButton.setButtonText ("CANCEL");
    setStatus ("Reading...");

    reader.start (slots,
        [this] (int done, int total)
        {
            setStatus ("Reading " + juce::String (done) + " / " + juce::String (total));
        },
        [this] (BankReader::Result result, bool complete)
        {
            for (auto& p : result)
                if (juce::isPositiveAndBelow (p.slot, (int) cache.size()))
                    cache[(size_t) p.slot] = p.patch;

            cacheTime = juce::Time::getCurrentTime();
            saveCache();
            readButton.setButtonText ("READ ALL");
            setStatus (complete ? "Read " + juce::String ((int) result.size()) + " sounds"
                                : "Stopped after " + juce::String ((int) result.size()) + " sounds");
        });
}

void SynthMemoryPanel::importBank()
{
    std::vector<DeviceDefinition::IncomingPatch> patches;

    for (int i = 0; i < slotsPerBank(); ++i)
    {
        auto slot = bank * slotsPerBank() + i;

        if (cache[(size_t) slot].has_value())
            patches.push_back ({ *cache[(size_t) slot], slot });
    }

    if (patches.empty())
    {
        setStatus ("Nothing to import - use READ ALL first");
        return;
    }

    auto r = library.importPatches (patches, "From Synth", "synth memory");
    setStatus ("Imported " + juce::String (r.imported) + " into \"From Synth\" (" + juce::String (r.duplicates) + " already there)");
}

//==============================================================================
bool SynthMemoryPanel::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith (dragPrefix);
}

void SynthMemoryPanel::itemDragEnter (const SourceDetails& d) { itemDragMove (d); }

void SynthMemoryPanel::itemDragMove (const SourceDetails& d)
{
    auto slot = slotAt (d.localPosition.toInt());

    if (slot != hoverSlot)
    {
        hoverSlot = slot;
        repaint();
    }
}

void SynthMemoryPanel::itemDragExit (const SourceDetails&)
{
    hoverSlot = -1;
    repaint();
}

void SynthMemoryPanel::itemDropped (const SourceDetails& d)
{
    auto slot = slotAt (d.localPosition.toInt());
    hoverSlot = -1;
    repaint();

    if (slot < 0)
        return;

    auto file = juce::File (d.description.toString().fromFirstOccurrenceOf (dragPrefix, false, false));

    if (auto preset = Preset::load (file))
        writePresetToSlot (*preset, slot);
}

//==============================================================================
juce::File SynthMemoryPanel::cacheFile() const
{
    return library.getRoot().getParentDirectory().getChildFile ("synth-memory.json");
}

void SynthMemoryPanel::loadCache()
{
    auto v = juce::JSON::parse (cacheFile());

    if (auto* slots = v.getProperty ("slots", {}).getDynamicObject())
        for (auto& prop : slots->getProperties())
            if (auto slot = prop.name.toString().getIntValue(); juce::isPositiveAndBelow (slot, (int) cache.size()))
                if (auto p = Patch::fromBase64 (prop.value.toString()); p.isValid())
                    cache[(size_t) slot] = p;

    cacheTime = juce::Time::fromISO8601 (v.getProperty ("read", {}).toString());

    if (cacheTime != juce::Time())
        status = "Memory as read on " + cacheTime.toString (true, true, false);
    else
        status = "Synth memory not read yet - click READ ALL";
}

void SynthMemoryPanel::saveCache() const
{
    auto* slots = new juce::DynamicObject();

    for (size_t i = 0; i < cache.size(); ++i)
        if (cache[i].has_value())
            slots->setProperty (juce::String ((int) i), cache[i]->toBase64());

    auto* root = new juce::DynamicObject();
    root->setProperty ("device", controller.getDevice().getDeviceId());
    root->setProperty ("read", cacheTime.toISO8601 (true));
    root->setProperty ("slots", juce::var (slots));

    cacheFile().getParentDirectory().createDirectory();
    cacheFile().replaceWithText (juce::JSON::toString (juce::var (root)));
}

} // namespace sso
