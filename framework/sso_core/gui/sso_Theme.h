#pragma once

namespace sso::theme
{
    // 1980s Roland-inspired console palette
    inline const juce::Colour background   { 0xff1b1b1d };
    inline const juce::Colour panel        { 0xff2a2a2d };
    inline const juce::Colour sidebar      { 0xff232326 };
    inline const juce::Colour control      { 0xff38383d };
    inline const juce::Colour controlHover { 0xff45454b };
    inline const juce::Colour slot         { 0xff111113 };
    inline const juce::Colour outline      { 0xff4a4a50 };
    inline const juce::Colour text         { 0xffeae3cf };
    inline const juce::Colour textDim      { 0xffa39d92 };
    inline const juce::Colour textOff      { 0xff77736b };
    inline const juce::Colour faderCap     { 0xffd9d2be };
    inline const juce::Colour faderCapTop  { 0xfff5efdf };

    inline const juce::Colour orange       { 0xfff28c28 };
    inline const juce::Colour red          { 0xffe0412f };
    inline const juce::Colour yellow       { 0xfff2c84b };
    inline const juce::Colour blue         { 0xff3e8ed0 };
    inline const juce::Colour cream        { 0xffeae3cf };

    inline const juce::Colour ledOn        { 0xfff2c84b };
    inline const juce::Colour ledOff       { 0xff4a4742 };

    inline const juce::Colour lcdBackground { 0xff0e1a12 };
    inline const juce::Colour lcdBorder     { 0xff2f3a30 };
    inline const juce::Colour lcdText       { 0xff8be07a };
    inline const juce::Colour lcdDim        { 0xff5fa453 };

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
