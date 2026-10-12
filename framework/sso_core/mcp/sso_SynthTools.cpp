namespace sso
{

namespace
{
    using ToolResult = McpServer::ToolResult;

    juce::var stringProperty (const juce::String& description)
    {
        return json::object ({ { "type", "string" }, { "description", description } });
    }

    juce::var integerProperty (const juce::String& description, int minimum, int maximum)
    {
        return json::object ({ { "type", "integer" }, { "description", description }, { "minimum", minimum }, { "maximum", maximum } });
    }

    juce::var keepProperty()
    {
        return json::object ({ { "type", "array" }, { "items", json::object ({ { "type", "string" } }) },
                               { "description", "Parameter groups to keep unchanged (see GENERATOR in synth_info)" } });
    }

    juce::String quoted (const juce::String& s)    { return "\"" + s + "\""; }
}

SynthTools::SynthTools (Context c)
    : context (c), device (c.controller.getDevice()), player (c.controller)
{
}

SynthTools::~SynthTools()
{
    player.stop();
}

//==============================================================================
juce::String SynthTools::instructions() const
{
    return context.appName + " controls a " + device.getDisplayName() + " hardware synthesizer over MIDI. "
           "Call synth_info first: it describes the synth and every parameter (id, range, function). "
           "Parameter changes reach the synth at once and show in the app. To let the user hear a sound, "
           "call play_notes - you cannot hear it yourself, so ask the user how it sounds. "
           "Nothing is ever stored in the synth's memory; save_preset stores sounds in the app's library.";
}

void SynthTools::addTo (McpServer& server)
{
    auto add = [&server, this] (const char* toolName, const char* toolTitle, juce::String description, juce::var schema,
                                ToolResult (SynthTools::*fn) (const juce::var&), bool readOnly, bool destructive = false)
    {
        server.addTool ({ toolName, toolTitle, std::move (description), std::move (schema), readOnly, destructive,
                          [this, fn] (const juce::var& args) { return (this->*fn) (args); } });
    };

    add ("synth_info", "Synth description",
         "The synth's architecture and all sound parameters as a compact table: id | section | range | function. "
         "Read this before changing anything.",
         json::schema(), &SynthTools::synthInfo, true);

    add ("get_sound", "Current sound",
         "The current sound: name, origin and every parameter value, grouped by section.",
         json::schema ({ { "section", stringProperty ("Only this section (optional)") } }),
         &SynthTools::getSound, true);

    add ("set_parameters", "Set parameters",
         "Sets one or more parameters of the current sound; the synth follows at once. "
         "values maps parameter ids to numbers, choice names (e.g. \"Saw\") or true/false.",
         json::schema ({ { "values", json::object ({ { "type", "object" },
                                                     { "description", "{ parameter id: value }, e.g. { \"cutoff\": 60 }" } }) } },
                       { "values" }),
         &SynthTools::setParameters, false);

    add ("set_sound_name", "Name the sound", "Renames the current sound.",
         json::schema ({ { "name", stringProperty ("New name") } }, { "name" }),
         &SynthTools::setSoundName, false);

    add ("init_sound", "Init sound", "Replaces the current sound with the plain init sound - a neutral starting point.",
         json::schema(), &SynthTools::initSound, false);

    auto noteItem = json::object ({ { "type", "object" },
                                    { "properties", json::object ({
                                        { "note", json::object ({ { "description", "MIDI number or name, C4 = 60 (A4 = 69 = 440 Hz)" } }) },
                                        { "start_ms", integerProperty ("Start; default: when the previous note ends", 0, (int) maxPhraseMs) },
                                        { "duration_ms", integerProperty ("Length, default 500", 10, 10000) },
                                        { "velocity", integerProperty ("1-127, default 100", 1, 127) } }) },
                                    { "required", json::array ({ "note" }) } });

    add ("play_notes", "Play notes",
         "Plays notes on the synth so the user can hear the sound (at most 30 s and 128 notes). "
         "Notes without start_ms follow each other (a melody); give chord notes the same start_ms. "
         "Or set phrase to \"test\" for the app's test phrase. Returns at once.",
         json::schema ({ { "notes", json::object ({ { "type", "array" }, { "items", noteItem } }) },
                         { "phrase", stringProperty ("\"test\" plays the test phrase instead of notes") } }),
         &SynthTools::playNotes, false);

    add ("stop_sound", "Stop sound", "Stops playing notes and silences the synth.",
         json::schema(), &SynthTools::stopSound, false);

    add ("list_presets", "List library sounds",
         "Searches the app's sound library. Each line: id | name | bank | category | tags.",
         json::schema ({ { "query", stringProperty ("Text in name, tags, category, author or comment") },
                         { "bank", stringProperty ("Only this bank") },
                         { "category", stringProperty ("Only this category") },
                         { "limit", integerProperty ("Lines to return, default 40", 1, 200) },
                         { "offset", integerProperty ("Skip this many matches", 0, 100000) } }),
         &SynthTools::listPresets, true);

    add ("load_preset", "Load library sound", "Loads a library sound (by id from list_presets, or by its unique name) "
                                              "as the current sound.",
         json::schema ({ { "id", stringProperty ("Preset id or name") } }, { "id" }),
         &SynthTools::loadPreset, false);

    add ("save_preset", "Save to library",
         juce::String ("Saves the current sound in the app's library (never in the synth). Default bank: \"") + aiBank
             + "\". An existing sound with the same name in that bank is only replaced with overwrite: true.",
         json::schema ({ { "name", stringProperty ("Sound name") },
                         { "bank", stringProperty (juce::String ("Library bank, default \"") + aiBank + "\"") },
                         { "category", stringProperty ("One of: " + Categories::all().joinIntoString (", ")) },
                         { "tags", json::object ({ { "type", "array" }, { "items", json::object ({ { "type", "string" } }) } }) },
                         { "comment", stringProperty ("Free text, e.g. how it was made") },
                         { "overwrite", json::object ({ { "type", "boolean" }, { "description", "Replace a sound with the same name" } }) } },
                       { "name" }),
         &SynthTools::savePreset, false, true);

    if (context.generator != nullptr)
    {
        add ("generate_sound", "Generate a sound",
             "Creates a random but playable sound of a category with the app's generator - a starting point to refine.",
             json::schema ({ { "category", stringProperty ("One of: " + context.generator->getCategories().joinIntoString (", ")) },
                             { "keep", keepProperty() } }, { "category" }),
             &SynthTools::generateSound, false);

        add ("vary_sound", "Vary the sound", "Changes the current sound a little (or more, with a higher amount).",
             json::schema ({ { "amount", json::object ({ { "type", "number" }, { "minimum", 0 }, { "maximum", 1 },
                                                         { "description", "0..1, default 0.3" } }) },
                             { "keep", keepProperty() } }),
             &SynthTools::varySound, false);
    }
}

//==============================================================================
int SynthTools::parseNote (const juce::var& v)
{
    if (v.isInt() || v.isInt64() || v.isDouble())
    {
        auto n = (double) v;
        return n == std::floor (n) && n >= 0 && n <= 127 ? (int) n : -1;
    }

    auto s = v.toString().trim();

    if (s.isEmpty())
        return -1;

    if (s.containsOnly ("0123456789"))
        return juce::isPositiveAndBelow (s.getIntValue(), 128) ? s.getIntValue() : -1;

    static const int semitones[] = { 9, 11, 0, 2, 4, 5, 7 };       // A B C D E F G
    auto letter = juce::CharacterFunctions::toUpperCase (s[0]);

    if (letter < 'A' || letter > 'G')
        return -1;

    auto pitch = semitones[letter - 'A'];
    auto rest = s.substring (1);

    while (rest.isNotEmpty() && (rest[0] == '#' || rest[0] == 'b'))
    {
        pitch += rest[0] == '#' ? 1 : -1;
        rest = rest.substring (1);
    }

    if (rest.isEmpty() || ! rest.trimCharactersAtStart ("-").containsOnly ("0123456789"))
        return -1;

    auto note = (rest.getIntValue() + 1) * 12 + pitch;
    return juce::isPositiveAndBelow (note, 128) ? note : -1;
}

std::optional<int> SynthTools::parseValue (const ParamSpec& spec, const juce::var& v)
{
    auto inRange = [&spec] (int x) { return x >= spec.minValue && x <= spec.maxValue ? std::optional<int> (x) : std::nullopt; };

    if (v.isBool())
        return spec.kind == ParamSpec::Kind::toggle ? std::optional<int> ((bool) v ? 1 : 0) : std::nullopt;

    if (v.isInt() || v.isInt64() || v.isDouble())
    {
        auto d = (double) v;
        return d == std::floor (d) ? inRange ((int) d) : std::nullopt;
    }

    auto s = v.toString().trim();

    if (spec.kind == ParamSpec::Kind::choice)
    {
        for (int i = 0; i < spec.choices.size(); ++i)
            if (spec.choices[i].equalsIgnoreCase (s))
                return spec.minValue + i;
    }

    if (spec.kind == ParamSpec::Kind::toggle)
    {
        if (s.equalsIgnoreCase ("on") || s.equalsIgnoreCase ("true"))   return 1;
        if (s.equalsIgnoreCase ("off") || s.equalsIgnoreCase ("false")) return 0;
    }

    if (s.isNotEmpty() && s.trimCharactersAtStart ("-").containsOnly ("0123456789"))
        return inRange (s.getIntValue());

    return std::nullopt;
}

juce::String SynthTools::rangeText (const ParamSpec& spec)
{
    switch (spec.kind)
    {
        case ParamSpec::Kind::choice:  return spec.choices.joinIntoString ("/");
        case ParamSpec::Kind::toggle:  return "off/on";
        case ParamSpec::Kind::bipolar: return juce::String (spec.minValue) + ".." + juce::String (spec.maxValue)
                                              + ", " + juce::String (spec.centreValue) + " = centre";
        case ParamSpec::Kind::continuous: break;
    }

    return juce::String (spec.minValue) + ".." + juce::String (spec.maxValue);
}

juce::String SynthTools::valueText (int index, int value) const
{
    auto& spec = device.getParams()[(size_t) index];

    if (spec.isDiscrete())
        return spec.kind == ParamSpec::Kind::toggle ? juce::String (value != 0 ? "on" : "off") : spec.toText (value);

    auto text = spec.toText (value);
    return text == juce::String (value) ? text : juce::String (value) + " (" + text + ")";
}

std::optional<ToolResult> SynthTools::refuseWhileBusy() const
{
    if (! context.controller.isActive())
        return ToolResult::error ("This app instance does not control the synth at the moment (another SSO instance does). "
                                  "Ask the user to press TAKE OVER in this instance.");

    if (context.controller.isTuning())
        return ToolResult::error ("The user is tuning the synth (TUNE is on). Try again when tuning is finished.");

    return std::nullopt;
}

//==============================================================================
ToolResult SynthTools::synthInfo (const juce::var&)
{
    juce::StringArray lines;
    auto firmware = device.getFirmwareInfo();
    lines.add ("SYNTH " + device.getDisplayName() + (firmware.isNotEmpty() ? " | firmware " + firmware : juce::String()));
    lines.add ("APP " + context.appName + " v" + context.appVersion);

    for (auto& line : juce::StringArray::fromLines (device.getSynthSummary()))
        if (line.trim().isNotEmpty())
            lines.add (line.trim());

    lines.add ("VALUES whole numbers in the range shown; choices by name (or number from the range start); off/on as false/true");
    lines.add ("NOTES MIDI numbers or names, C4 = 60, A4 = 69 = 440 Hz");

    if (context.generator != nullptr)
        lines.add ("GENERATOR categories " + context.generator->getCategories().joinIntoString (", ")
                   + " | keep groups " + context.generator->getGroups().joinIntoString (", "));

    lines.add ("PARAMS id | section | range | function");

    for (auto& spec : device.getParams())
        lines.add (spec.id + " | " + spec.section + " | " + rangeText (spec) + " | " + spec.help);

    return { lines.joinIntoString ("\n") };
}

juce::String SynthTools::soundSummary() const
{
    auto& controller = context.controller;
    auto& ref = controller.getPresetRef();
    juce::String origin = ref.bank.isNotEmpty() ? "bank " + quoted (ref.bank) : juce::String ("not from the library");

    if (ref.category.isNotEmpty())
        origin << ", category " << ref.category;

    auto name = controller.getPatchName();
    return "SOUND " + quoted (name.isNotEmpty() ? name : juce::String ("(no name)")) + " | " + origin;
}

ToolResult SynthTools::getSound (const juce::var& args)
{
    auto onlySection = args["section"].toString();

    if (onlySection.isNotEmpty() && ! device.getSections().contains (onlySection, true))
        return ToolResult::error ("Unknown section " + quoted (onlySection) + ". Sections: " + device.getSections().joinIntoString (", "));

    juce::StringArray lines { soundSummary() };
    auto values = json::object();

    for (auto& section : device.getSections())
    {
        if (onlySection.isNotEmpty() && ! section.equalsIgnoreCase (onlySection))
            continue;

        juce::StringArray items;

        for (int i = 0; i < (int) device.getParams().size(); ++i)
        {
            auto& spec = device.getParams()[(size_t) i];

            if (spec.section != section)
                continue;

            auto value = context.controller.getValue (i);
            items.add (spec.id + "=" + valueText (i, value));
            values.getDynamicObject()->setProperty (spec.id, value);
        }

        lines.add ("[" + section + "] " + items.joinIntoString ("  "));
    }

    auto& ref = context.controller.getPresetRef();
    return { lines.joinIntoString ("\n"),
             json::object ({ { "name", context.controller.getPatchName() }, { "bank", ref.bank },
                             { "category", ref.category }, { "values", values } }) };
}

ToolResult SynthTools::setParameters (const juce::var& args)
{
    if (auto refusal = refuseWhileBusy())
        return *refusal;

    auto* values = args["values"].getDynamicObject();

    if (values == nullptr || values->getProperties().isEmpty())
        return ToolResult::error ("values must be an object like { \"cutoff\": 60 }.");

    juce::StringArray done, problems;

    for (auto& property : values->getProperties())
    {
        auto id = property.name.toString();
        auto index = device.indexOf (id);

        if (index < 0)
        {
            problems.add (id + ": unknown parameter (see synth_info)");
            continue;
        }

        auto& spec = device.getParams()[(size_t) index];
        auto value = parseValue (spec, property.value);

        if (! value.has_value())
        {
            problems.add (id + ": " + property.value.toString() + " is not valid, range " + rangeText (spec));
            continue;
        }

        context.controller.setValue (index, *value);
        done.add (id + "=" + valueText (index, *value));
    }

    juce::String text;

    if (! done.isEmpty())
        text << "Set " << done.joinIntoString ("  ");

    if (! problems.isEmpty())
        text << (text.isEmpty() ? "" : "\n") << "Not set: " << problems.joinIntoString ("; ");

    return { text, {}, done.isEmpty() };
}

ToolResult SynthTools::setSoundName (const juce::var& args)
{
    auto name = args["name"].toString().trim();

    if (name.isEmpty())
        return ToolResult::error ("The name must not be empty.");

    context.controller.setPatchName (name);
    auto stored = context.controller.getPatchName();
    return { "Name " + quoted (stored) + (stored != name ? " (shortened to the synth's " + juce::String (device.getMaxNameLength())
                                                           + " characters)" : juce::String()) };
}

ToolResult SynthTools::initSound (const juce::var&)
{
    if (auto refusal = refuseWhileBusy())
        return *refusal;

    context.controller.silence();
    context.controller.loadPatch (device.createInitPatch(), true);
    context.controller.setPresetRef ({});
    return { "Init sound loaded.\n" + getSound ({}).text };
}

ToolResult SynthTools::playNotes (const juce::var& args)
{
    if (auto refusal = refuseWhileBusy())
        return *refusal;

    if (args["phrase"].toString().equalsIgnoreCase ("test"))
    {
        player.play();
        return { "Playing the test phrase (" + juce::String (player.getPhraseLengthMs() / 1000.0, 1) + " s)." };
    }

    auto* notes = args["notes"].getArray();

    if (notes == nullptr || notes->isEmpty())
        return ToolResult::error ("Give notes, e.g. [{ \"note\": \"C4\", \"duration_ms\": 800 }], or phrase \"test\".");

    if (notes->size() > maxNotes)
        return ToolResult::error ("At most " + juce::String (maxNotes) + " notes per call.");

    std::vector<TestNotePlayer::Event> events;
    double previousEnd = 0, end = 0;

    for (int i = 0; i < notes->size(); ++i)
    {
        auto& n = notes->getReference (i);
        auto note = parseNote (n.isObject() ? n["note"] : n);

        if (note < 0)
            return ToolResult::error ("Note " + juce::String (i + 1) + ": " + (n.isObject() ? n["note"] : n).toString()
                                      + " is not a note (0-127 or a name like C4, F#3, Bb2).");

        auto start = n.isObject() && n.hasProperty ("start_ms") ? (double) n["start_ms"] : previousEnd;
        auto length = n.isObject() && n.hasProperty ("duration_ms") ? (double) n["duration_ms"] : 500.0;
        auto velocity = n.isObject() && n.hasProperty ("velocity") ? (int) n["velocity"] : 100;

        if (start < 0 || length < 10 || length > 10000 || ! juce::isPositiveAndBelow (velocity - 1, 127))
            return ToolResult::error ("Note " + juce::String (i + 1) + ": start_ms >= 0, duration_ms 10-10000, velocity 1-127.");

        events.push_back ({ start, note, true, velocity });
        events.push_back ({ start + length, note, false, 0 });
        previousEnd = start + length;
        end = juce::jmax (end, previousEnd);
    }

    if (end > maxPhraseMs)
        return ToolResult::error ("The notes take " + juce::String (end / 1000.0, 1) + " s; at most 30 s per call.");

    player.play (std::move (events));
    return { "Playing " + juce::String (notes->size()) + " note" + (notes->size() == 1 ? "" : "s") + " ("
             + juce::String (end / 1000.0, 1) + " s). Ask the user how it sounds." };
}

ToolResult SynthTools::stopSound (const juce::var&)
{
    player.stop();

    if (context.controller.isActive() && ! context.controller.isTuning())
        context.controller.silence();

    return { "Stopped." };
}

//==============================================================================
juce::String SynthTools::presetId (const Preset& p) const
{
    return p.file.getRelativePathFrom (context.library.getRoot()).replaceCharacter ('\\', '/')
               .upToLastOccurrenceOf (Preset::fileExtension, false, true);
}

const Preset* SynthTools::findPreset (const juce::String& idOrName, juce::String& problem) const
{
    auto& presets = context.library.getPresets();

    for (auto& p : presets)
        if (presetId (p) == idOrName)
            return &p;

    std::vector<const Preset*> named;

    for (auto& p : presets)
        if (p.name.equalsIgnoreCase (idOrName))
            named.push_back (&p);

    if (named.size() == 1)
        return named.front();

    if (named.empty())
        problem = "No library sound " + quoted (idOrName) + " - use an id from list_presets.";
    else
    {
        juce::StringArray ids;

        for (auto* p : named)
            ids.add (presetId (*p));

        problem = "Several sounds are called " + quoted (idOrName) + ": " + ids.joinIntoString (", ") + " - give the id.";
    }

    return nullptr;
}

ToolResult SynthTools::listPresets (const juce::var& args)
{
    auto& library = context.library;
    PresetLibrary::Filter filter;
    filter.text = args["query"].toString();
    filter.bank = args["bank"].toString();
    filter.category = args["category"].toString();

    auto matches = library.search (filter);
    auto limit = args.hasProperty ("limit") ? juce::jlimit (1, 200, (int) args["limit"]) : 40;
    auto offset = args.hasProperty ("offset") ? juce::jmax (0, (int) args["offset"]) : 0;

    juce::StringArray lines;
    lines.add ("PRESETS " + juce::String ((int) matches.size()) + " match | banks " + library.getBanks().joinIntoString (", ")
               + " | categories " + Categories::all().joinIntoString (", "));

    for (int i = offset; i < juce::jmin ((int) matches.size(), offset + limit); ++i)
    {
        auto& p = library.getPresets()[(size_t) matches[(size_t) i]];
        lines.add (presetId (p) + " | " + p.name + " | " + p.bank + " | " + p.category + " | " + p.tags.joinIntoString (","));
    }

    if (offset + limit < (int) matches.size())
        lines.add ("... " + juce::String ((int) matches.size() - offset - limit) + " more (use offset)");

    return { lines.joinIntoString ("\n") };
}

ToolResult SynthTools::loadPreset (const juce::var& args)
{
    if (auto refusal = refuseWhileBusy())
        return *refusal;

    juce::String problem;
    auto* p = findPreset (args["id"].toString().trim(), problem);

    if (p == nullptr)
        return ToolResult::error (problem);

    auto preset = *p;   // the library may rescan
    context.controller.silence();
    context.controller.loadPatch (preset.patch, true);
    context.controller.setPresetRef ({ preset.file, preset.bank, preset.category });
    return { "Loaded.\n" + getSound ({}).text };
}

ToolResult SynthTools::savePreset (const juce::var& args)
{
    auto name = args["name"].toString().trim();

    if (name.isEmpty())
        return ToolResult::error ("The name must not be empty.");

    auto bank = juce::File::createLegalFileName (args["bank"].toString().trim());

    if (bank.isEmpty() || bank.containsOnly ("."))
        bank = aiBank;

    auto category = args["category"].toString().trim();

    if (category.isNotEmpty())
    {
        auto index = Categories::all().indexOf (category, true);

        if (index < 0)
            return ToolResult::error ("Unknown category " + quoted (category) + ". Categories: " + Categories::all().joinIntoString (", "));

        category = Categories::all()[index];
    }
    else
    {
        category = Categories::guessFromName (name);
    }

    Preset p;
    p.name = name;
    p.category = category;
    p.comment = args["comment"].toString();
    p.origin = "Made with AI control (MCP)";

    if (auto* tags = args["tags"].getArray())
        for (auto& t : *tags)
            if (t.toString().trim().isNotEmpty())
                p.tags.add (t.toString().trim());

    p.patch = context.controller.getCurrentPatch();
    device.setPatchName (p.patch, name);

    auto& library = context.library;
    const Preset* existing = nullptr;

    for (auto& other : library.getPresets())
        if (other.bank == bank && other.name.equalsIgnoreCase (name))
            existing = &other;

    juce::File file;

    if (existing != nullptr)
    {
        if (! (bool) args["overwrite"])
            return ToolResult::error ("Bank " + quoted (bank) + " already has a sound " + quoted (name)
                                      + ". Choose another name, or set overwrite: true to replace it.");

        // the new sound; category, tags and comment only where given
        auto updated = *existing;
        updated.patch = p.patch;
        updated.origin = p.origin;

        if (args.hasProperty ("category")) updated.category = p.category;
        if (args.hasProperty ("comment"))  updated.comment = p.comment;
        if (args.hasProperty ("tags"))     updated.tags = p.tags;

        category = updated.category;

        if (! library.update (updated))
            return ToolResult::error ("Could not write " + updated.file.getFullPathName());

        file = updated.file;
    }
    else
    {
        file = library.add (p, bank);

        if (file == juce::File())
            return ToolResult::error ("Could not save the sound in bank " + quoted (bank) + ".");
    }

    context.controller.setPatchName (name);
    context.controller.setPresetRef ({ file, bank, category });
    return { "Saved " + quoted (name) + " in bank " + quoted (bank) + " (" + category + ")" + (existing != nullptr ? ", replacing the old one." : ".") };
}

//==============================================================================
juce::StringArray SynthTools::keepGroups (const juce::var& v, juce::String& problem) const
{
    juce::StringArray keep;

    if (auto* list = v.getArray())
    {
        for (auto& g : *list)
        {
            auto index = context.generator->getGroups().indexOf (g.toString(), true);

            if (index < 0)
            {
                problem = "Unknown group " + quoted (g.toString()) + ". Groups: " + context.generator->getGroups().joinIntoString (", ");
                return {};
            }

            keep.add (context.generator->getGroups()[index]);
        }
    }

    return keep;
}

void SynthTools::applyGenerated (const Patch& patch, const juce::String& category)
{
    context.controller.silence();
    context.controller.loadPatch (patch, true);
    context.controller.setPresetRef ({ {}, "Generated", category });
}

ToolResult SynthTools::generateSound (const juce::var& args)
{
    if (auto refusal = refuseWhileBusy())
        return *refusal;

    auto categories = context.generator->getCategories();
    auto index = categories.indexOf (args["category"].toString().trim(), true);

    if (index < 0)
        return ToolResult::error ("Unknown category. Categories: " + categories.joinIntoString (", "));

    juce::String problem;
    auto keep = keepGroups (args["keep"], problem);

    if (problem.isNotEmpty())
        return ToolResult::error (problem);

    applyGenerated (context.generator->generate (categories[index], context.controller.getCurrentPatch(), keep, random),
                    categories[index]);
    return { "Generated.\n" + getSound ({}).text };
}

ToolResult SynthTools::varySound (const juce::var& args)
{
    if (auto refusal = refuseWhileBusy())
        return *refusal;

    auto amount = args.hasProperty ("amount") ? (float) (double) args["amount"] : 0.3f;

    if (amount < 0.0f || amount > 1.0f)
        return ToolResult::error ("amount must be between 0 and 1.");

    juce::String problem;
    auto keep = keepGroups (args["keep"], problem);

    if (problem.isNotEmpty())
        return ToolResult::error (problem);

    applyGenerated (context.generator->vary (context.controller.getCurrentPatch(), amount, keep, random),
                    context.controller.getPresetRef().category);
    return { "Varied.\n" + getSound ({}).text };
}

} // namespace sso
