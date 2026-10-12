#pragma once

namespace sso::theme
{
    // Default: 1980s Roland-inspired console palette. A programmer can switch to another
    // palette once at startup (each plugin binary has its own copy of these values).
    inline juce::Colour background   { 0xff1b1b1d };
    inline juce::Colour panel        { 0xff2a2a2d };
    inline juce::Colour sidebar      { 0xff232326 };
    inline juce::Colour control      { 0xff38383d };
    inline juce::Colour controlHover { 0xff45454b };
    inline juce::Colour slot         { 0xff111113 };
    inline juce::Colour outline      { 0xff4a4a50 };
    inline juce::Colour text         { 0xffeae3cf };
    inline juce::Colour textDim      { 0xffa39d92 };
    inline juce::Colour textOff      { 0xff77736b };
    inline juce::Colour faderCap     { 0xffd9d2be };
    inline juce::Colour faderCapTop  { 0xfff5efdf };

    inline juce::Colour orange       { 0xfff28c28 };
    inline juce::Colour red          { 0xffe0412f };
    inline juce::Colour yellow       { 0xfff2c84b };
    inline juce::Colour blue         { 0xff3e8ed0 };
    inline juce::Colour cream        { 0xffeae3cf };

    inline juce::Colour accent       { 0xfff2c84b };    // selections, active chips, hover targets
    inline juce::Colour ledOn        { 0xfff2c84b };
    inline juce::Colour ledOff       { 0xff4a4742 };

    inline juce::Colour lcdBackground { 0xff0e1a12 };
    inline juce::Colour lcdBorder     { 0xff2f3a30 };
    inline juce::Colour lcdText       { 0xff8be07a };
    inline juce::Colour lcdDim        { 0xff5fa453 };

    // Rotary knobs
    inline juce::Colour knobCap      { 0xffe9e0c8 };
    inline juce::Colour knobSkirt    { 0xffb9c0cc };
    inline juce::Colour knobPointer  { 0xff1a1d24 };
    inline juce::Colour knobArc      { 0xffe8742c };

    // Section frames: coloured stripe (default) or a cream title tab with an outline
    inline bool tabbedSections = false;
    inline juce::Colour sectionOutline { 0xff7c8aa6 };
    inline juce::Colour tabFill        { 0xffede6d6 };
    inline juce::Colour tabText        { 0xff121110 };

    // Optional decoration: black strip behind the header, wooden end cheeks
    inline bool headerStrip = false;
    inline juce::Colour headerStripColour { 0xff0a0c11 };
    inline bool woodCheeks = false;
    inline juce::Colour wood { 0xff5c3820 };

    /** Korg PolySix: navy panel (measured from a photo of the original, ~#232D42), black top
        strip, cream knob caps with silver skirts, red LEDs and display, wooden cheeks. */
    inline void usePolySixPalette()
    {
        background   = juce::Colour (0xff172238);
        panel        = juce::Colour (0xff1f2d4a);
        sidebar      = juce::Colour (0xff111a2c);
        control      = juce::Colour (0xff26334f);
        controlHover = juce::Colour (0xff304063);
        slot         = juce::Colour (0xff0d1424);
        outline      = juce::Colour (0xff3a4866);
        text         = juce::Colour (0xffede6d6);
        textDim      = juce::Colour (0xffa9b4c9);
        textOff      = juce::Colour (0xff6f7c96);

        orange       = juce::Colour (0xffe8742c);
        red          = juce::Colour (0xffd83a26);
        accent       = orange;
        ledOn        = juce::Colour (0xffff3b1f);
        ledOff       = juce::Colour (0xff3a2a38);

        lcdBackground = juce::Colour (0xff1a0705);
        lcdBorder     = juce::Colour (0xff3a1a14);
        lcdText       = juce::Colour (0xffff5a3a);
        lcdDim        = juce::Colour (0xffb8402a);

        tabbedSections = true;
        headerStrip = true;
        woodCheeks = true;
    }

    /** Korg Poly-800: black plastic case with white print, red LED display. Dark knobs with
        white pointers, light blue value arcs and selections. */
    inline void usePoly800Palette()
    {
        background   = juce::Colour (0xff0e0f11);
        panel        = juce::Colour (0xff15161a);
        sidebar      = juce::Colour (0xff08090a);
        control      = juce::Colour (0xff22242a);
        controlHover = juce::Colour (0xff2c2f36);
        slot         = juce::Colour (0xff050506);
        outline      = juce::Colour (0xff3a3d45);
        text         = juce::Colour (0xffecece6);
        textDim      = juce::Colour (0xff9ea1a9);
        textOff      = juce::Colour (0xff5c5f68);

        orange       = juce::Colour (0xffe8742c);
        red          = juce::Colour (0xffd83a26);
        blue         = juce::Colour (0xff5ab0e8);
        accent       = blue;
        ledOn        = juce::Colour (0xffff3b2a);
        ledOff       = juce::Colour (0xff3a1a16);

        lcdBackground = juce::Colour (0xff1a0504);
        lcdBorder     = juce::Colour (0xff3a1410);
        lcdText       = juce::Colour (0xffff5a3a);
        lcdDim        = juce::Colour (0xffb8402a);

        knobCap     = juce::Colour (0xff2a2c31);
        knobSkirt   = juce::Colour (0xff55585f);
        knobPointer = juce::Colour (0xffecece6);
        knobArc     = blue;

        tabbedSections = true;
        sectionOutline = juce::Colour (0xff55585f);
        tabFill        = juce::Colour (0xffecece6);
        tabText        = juce::Colour (0xff0e0f11);
        headerStrip = true;
        headerStripColour = juce::Colour (0xff050506);
        woodCheeks = false;
    }

    /** Condensed UI font (Bahnschrift ships with Windows 10+, falls back to the default sans). */
    inline juce::Font label (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Bahnschrift", height, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font lcd (float height)
    {
        return juce::Font (juce::FontOptions ("Consolas", height, juce::Font::plain));
    }
}
