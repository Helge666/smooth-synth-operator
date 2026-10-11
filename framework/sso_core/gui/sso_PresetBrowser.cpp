namespace sso
{

static const juce::String favouritesChip = juce::String::charToString (0x2605);   // black star

PresetBrowser::PresetBrowser (PresetLibrary& lib, SynthController& c)
    : library (lib), controller (c), memory (lib, c)
{
    search.setTextToShowWhenEmpty ("Search name, tag, author...", theme::textOff);
    search.setFont (theme::label (15.0f));
    search.setIndents (10, 8);
    search.onTextChange = [this] { filter.text = search.getText(); refreshResults(); };
    search.onEscapeKey = [this] { search.clear(); filter.text = {}; refreshResults(); };
    addAndMakeVisible (search);

    list.setModel (this);
    list.setRowHeight (rowHeight);
    list.setMultipleSelectionEnabled (false);
    addAndMakeVisible (list);

    saveButton.setTooltip ("Overwrite the library preset the current sound came from");
    saveButton.onClick = [this] { saveCurrent(); };
    saveAsButton.onClick = [this] { saveAs(); };
    importButton.setTooltip ("Import sounds from a SysEx file (.syx)");
    importButton.onClick = [this] { importSyx(); };

    for (auto* b : { &saveButton, &saveAsButton, &importButton })
        addAndMakeVisible (b);

    addAndMakeVisible (memory);
    memory.onOpenChanged = [this] { resized(); };

    library.addChangeListener (this);
    controller.addChangeListener (this);
    rebuildFilterButtons();
    refreshResults();
}

PresetBrowser::~PresetBrowser()
{
    library.removeChangeListener (this);
    controller.removeChangeListener (this);
}

void PresetBrowser::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &library)
    {
        if (library.getBanks() != shownBanks)
            rebuildFilterButtons();

        refreshResults();
    }

    list.repaint();   // highlight of the current preset
}

void PresetBrowser::rebuildFilterButtons()
{
    // Never call this from a chip's own onClick: it deletes the buttons.
    bankButtons.clear();
    categoryButtons.clear();
    bankValues.clear();
    categoryValues.clear();

    auto makeChip = [this] (juce::OwnedArray<juce::TextButton>& array, const juce::String& text, std::function<void()> onClick)
    {
        auto* b = array.add (new juce::TextButton (text));
        b->setClickingTogglesState (false);
        b->onClick = std::move (onClick);
        addAndMakeVisible (b);
    };

    shownBanks = library.getBanks();

    if (filter.bank.isNotEmpty() && ! shownBanks.contains (filter.bank))
        filter.bank = {};

    auto banks = shownBanks;
    banks.insert (0, {});

    for (auto& bank : banks)
    {
        bankValues.add (bank);
        makeChip (bankButtons, bank.isEmpty() ? juce::String ("All") : bank, [this, bank] { selectBank (bank); });
    }

    auto categories = Categories::all();
    categories.insert (0, {});

    for (auto& cat : categories)
    {
        categoryValues.add (cat);
        makeChip (categoryButtons, cat.isEmpty() ? juce::String ("All") : cat, [this, cat] { selectCategory (cat); });
    }

    makeChip (categoryButtons, favouritesChip, [this] { toggleFavourites(); });

    updateFilterButtonStates();
    resized();
}

void PresetBrowser::updateFilterButtonStates()
{
    for (int i = 0; i < bankButtons.size(); ++i)
        bankButtons[i]->setToggleState (bankValues[i] == filter.bank, juce::dontSendNotification);

    // the favourites chip is the last category button and has no category value
    for (int i = 0; i < categoryButtons.size(); ++i)
    {
        auto isFavourites = i >= categoryValues.size();
        categoryButtons[i]->setToggleState (isFavourites ? filter.favoritesOnly
                                                         : (categoryValues[i] == filter.category && ! (categoryValues[i].isEmpty() && filter.favoritesOnly)),
                                            juce::dontSendNotification);
    }
}

void PresetBrowser::selectBank (const juce::String& bank)
{
    filter.bank = bank;
    updateFilterButtonStates();
    refreshResults();
}

void PresetBrowser::selectCategory (const juce::String& category)
{
    filter.category = category;
    filter.favoritesOnly = false;
    updateFilterButtonStates();
    refreshResults();
}

void PresetBrowser::toggleFavourites()
{
    filter.favoritesOnly = ! filter.favoritesOnly;
    updateFilterButtonStates();
    refreshResults();
}

void PresetBrowser::refreshResults()
{
    results = library.search (filter);
    list.updateContent();
    list.repaint();
}

const Preset* PresetBrowser::presetForRow (int row) const
{
    if (! juce::isPositiveAndBelow (row, (int) results.size()))
        return nullptr;

    return &library.getPresets()[(size_t) results[(size_t) row]];
}

//==============================================================================
int PresetBrowser::getNumRows()
{
    return (int) results.size();
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    auto* p = presetForRow (row);

    if (p == nullptr)
        return;

    auto isCurrent = p->file == controller.getPresetRef().file;
    auto r = juce::Rectangle<int> (0, 0, width, height).reduced (0, 1);

    if (selected || isCurrent)
    {
        g.setColour (isCurrent ? theme::controlHover : theme::control);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }

    r.reduce (8, 0);

    g.setColour (theme::accent);
    g.setFont (theme::label (14.0f));
    g.drawText (p->favorite ? favouritesChip : juce::String(), r.removeFromRight (16), juce::Justification::centredRight);

    g.setColour (theme::textDim);
    g.setFont (theme::label (12.0f));
    g.drawText (p->category, r.removeFromRight (56), juce::Justification::centredRight);

    if (filter.bank.isEmpty())
        g.drawText (p->bank, r.removeFromRight (80), juce::Justification::centredRight);

    g.setColour (isCurrent ? theme::accent : theme::text);
    g.setFont (theme::label (15.0f, isCurrent));
    g.drawFittedText (p->name, r, juce::Justification::centredLeft, 1, 0.8f);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (auto* p = presetForRow (row))
    {
        if (e.mods.isPopupMenu())
            showPresetMenu (*p);
        else
            loadPreset (*p);
    }
}

void PresetBrowser::returnKeyPressed (int row)
{
    if (auto* p = presetForRow (row))
        loadPreset (*p);
}

juce::var PresetBrowser::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    if (auto* p = presetForRow (rows.isEmpty() ? -1 : rows[0]))
        return juce::String (SynthMemoryPanel::dragPrefix) + p->file.getFullPathName();

    return {};
}

juce::String PresetBrowser::getTooltipForRow (int row)
{
    if (auto* p = presetForRow (row))
    {
        juce::StringArray lines { p->name };

        if (p->tags.size() > 0)  lines.add ("Tags: " + p->tags.joinIntoString (", "));
        if (p->author.isNotEmpty())  lines.add ("By " + p->author);
        if (p->origin.isNotEmpty())  lines.add ("From " + p->origin);
        if (p->comment.isNotEmpty()) lines.add (p->comment);
        lines.add ("Drag onto a synth memory slot to store it.");
        return lines.joinIntoString ("\n");
    }

    return {};
}

//==============================================================================
void PresetBrowser::loadPreset (const Preset& p)
{
    controller.silence();
    controller.loadPatch (p.patch, true);
    controller.setPresetRef ({ p.file, p.bank, p.category });
}

void PresetBrowser::showPresetMenu (const Preset& preset)
{
    juce::PopupMenu menu;
    auto copy = preset;

    menu.addItem ("Load", [this, copy] { loadPreset (copy); });
    menu.addItem (copy.favorite ? "Remove from favorites" : "Add to favorites", [this, copy]() mutable
    {
        copy.favorite = ! copy.favorite;
        library.update (copy);
    });

    juce::PopupMenu categories;
    for (auto& cat : Categories::all())
        categories.addItem (cat, true, cat == copy.category, [this, copy, cat]() mutable
        {
            copy.category = cat;
            library.update (copy);
        });
    menu.addSubMenu ("Category", categories);

    juce::PopupMenu rating;
    for (int stars = 0; stars <= 5; ++stars)
        rating.addItem (stars == 0 ? juce::String ("None") : juce::String::repeatedString (favouritesChip, stars),
                        true, copy.rating == stars, [this, copy, stars]() mutable
        {
            copy.rating = stars;
            library.update (copy);
        });
    menu.addSubMenu ("Rating", rating);

    menu.addItem ("Rename...", [this, copy]
    {
        askForText ("Rename", "New name:", copy.name, [this, copy] (juce::String name) mutable
        {
            copy.name = name;
            controller.getDevice().setPatchName (copy.patch, name);
            library.update (copy);
        });
    });

    menu.addItem ("Edit tags...", [this, copy]
    {
        askForText ("Tags", "Comma separated tags:", copy.tags.joinIntoString (", "), [this, copy] (juce::String text) mutable
        {
            copy.tags = juce::StringArray::fromTokens (text, ",", {});
            copy.tags.trim();
            copy.tags.removeEmptyStrings();
            library.update (copy);
        });
    });

    menu.addSeparator();
    menu.addItem ("Show in Explorer", [copy] { copy.file.revealToUser(); });
    menu.addItem ("Move to trash", [this, copy]
    {
        auto options = juce::MessageBoxOptions()
                           .withIconType (juce::MessageBoxIconType::QuestionIcon)
                           .withTitle ("Move to trash")
                           .withMessage ("Move \"" + copy.name + "\" to the trash?")
                           .withButton ("Move to trash")
                           .withButton ("Cancel")
                           .withAssociatedComponent (this);

        messageBox = juce::NativeMessageBox::showScopedAsync (options, [this, copy] (int result)
        {
            if (result == 1)
                library.remove (copy);
        });
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

void PresetBrowser::askForText (const juce::String& title, const juce::String& message, const juce::String& initial,
                                std::function<void (juce::String)> onOk)
{
    dialog = std::make_unique<juce::AlertWindow> (title, message, juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor ("text", initial);
    dialog->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    showDialog ([this, onOk] (int result)
    {
        auto text = dialog->getTextEditorContents ("text").trim();

        if (result == 1 && text.isNotEmpty())
            onOk (text);
    });
}

void PresetBrowser::showDialog (std::function<void (int)> onResult)
{
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this, onResult] (int result)
    {
        onResult (result);
        closeDialog();
    }), false);
}

void PresetBrowser::closeDialog()
{
    if (dialog == nullptr)
        return;

    // The window stays on screen unless hidden; delete it after the modal callback has returned.
    dialog->setVisible (false);
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PresetBrowser> (this), shown = dialog.get()]
    {
        if (safe != nullptr && safe->dialog.get() == shown)
            safe->dialog.reset();
    });
}

void PresetBrowser::renameCurrent()
{
    askForText ("Rename", "Name of the current sound (max. " + juce::String (controller.getDevice().getMaxNameLength())
                              + " characters are stored in the synth):",
                controller.getPatchName(), [this] (juce::String name) { controller.setPatchName (name); });
}

void PresetBrowser::saveCurrent()
{
    auto ref = controller.getPresetRef();
    auto existing = Preset::load (ref.file);

    if (! existing.has_value())
    {
        saveAs();
        return;
    }

    existing->patch = controller.getCurrentPatch();
    existing->name = controller.getPatchName();
    existing->bank = ref.bank;
    library.update (*existing);
}

void PresetBrowser::saveAs()
{
    dialog = std::make_unique<juce::AlertWindow> ("Save sound", "Save the current sound to the library.",
                                                  juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor ("name", controller.getPatchName(), "Name");

    auto banks = library.getBanks();
    banks.addIfNotAlreadyThere ("My Sounds");
    dialog->addComboBox ("bank", banks, "Bank");
    auto currentBank = controller.getPresetRef().bank;
    dialog->getComboBoxComponent ("bank")->setText (currentBank.isNotEmpty() && ! currentBank.endsWithIgnoreCase ("Factory") ? currentBank
                                                                                                               : juce::String ("My Sounds"));
    dialog->getComboBoxComponent ("bank")->setEditableText (true);

    dialog->addComboBox ("category", Categories::all(), "Category");
    auto category = controller.getPresetRef().category;
    dialog->getComboBoxComponent ("category")->setText (category.isNotEmpty() ? category
                                                                              : Categories::guessFromName (controller.getPatchName()));
    dialog->addTextEditor ("tags", {}, "Tags (comma separated)");

    dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    showDialog ([this] (int result)
    {
        if (result != 1)
            return;

        Preset p;
        p.name = dialog->getTextEditorContents ("name").trim();
        p.category = dialog->getComboBoxComponent ("category")->getText();
        p.tags = juce::StringArray::fromTokens (dialog->getTextEditorContents ("tags"), ",", {});
        p.tags.trim();
        p.tags.removeEmptyStrings();
        p.origin = "Edited in SSO";
        p.patch = controller.getCurrentPatch();
        controller.getDevice().setPatchName (p.patch, p.name);

        auto bank = dialog->getComboBoxComponent ("bank")->getText().trim();
        auto file = library.add (p, bank.isEmpty() ? juce::String ("My Sounds") : bank);

        if (file != juce::File())
        {
            controller.setPatchName (p.name);
            controller.setPresetRef ({ file, bank, p.category });
        }
    });
}

void PresetBrowser::importSyx()
{
    chooser = std::make_unique<juce::FileChooser> ("Import SysEx", juce::File(), "*.syx");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (! file.existsAsFile())
            return;

        auto result = library.importSyx (file, file.getFileNameWithoutExtension());

        auto options = juce::MessageBoxOptions()
                           .withIconType (juce::MessageBoxIconType::InfoIcon)
                           .withTitle ("Import")
                           .withMessage ("Imported " + juce::String (result.imported) + " sounds into the bank \""
                                         + file.getFileNameWithoutExtension() + "\".\n"
                                         + juce::String (result.duplicates) + " were already there, "
                                         + juce::String (result.rejected) + " messages were not sounds.")
                           .withButton ("OK")
                           .withAssociatedComponent (this);

        messageBox = juce::NativeMessageBox::showScopedAsync (options, nullptr);
    });
}

//==============================================================================
void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (theme::sidebar);
    g.setColour (theme::control);
    g.fillRect (getLocalBounds().removeFromRight (1));

    g.setColour (theme::textDim);
    g.setFont (theme::label (12.0f));
    g.drawText ("LIBRARY", 14, 14, 200, 16, juce::Justification::centredLeft);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (14, 0).withTrimmedTop (34);

    search.setBounds (r.removeFromTop (34));
    r.removeFromTop (8);

    auto layoutChips = [&r] (juce::OwnedArray<juce::TextButton>& chips, int height)
    {
        int x = r.getX(), y = r.getY();

        for (auto* b : chips)
        {
            auto w = juce::jmax (34, juce::GlyphArrangement::getStringWidthInt (theme::label (13.0f, true), b->getButtonText()) + 18);

            if (x + w > r.getRight())
            {
                x = r.getX();
                y += height + 4;
            }

            b->setBounds (x, y, w, height);
            x += w + 4;
        }

        r.setTop (y + height + 8);
    };

    layoutChips (bankButtons, 28);
    layoutChips (categoryButtons, 24);

    auto bottom = getLocalBounds().removeFromBottom (memory.getPreferredHeight() + 20);
    memory.setBounds (bottom.reduced (10, 10));

    r.setBottom (bottom.getY());
    auto buttons = r.removeFromBottom (34);
    r.removeFromBottom (6);
    auto w = buttons.getWidth() / 3;
    saveButton.setBounds (buttons.removeFromLeft (w).withTrimmedRight (3));
    saveAsButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    importButton.setBounds (buttons.withTrimmedLeft (3));

    list.setBounds (r);
}

} // namespace sso
