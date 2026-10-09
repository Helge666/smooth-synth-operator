namespace sso
{

LookAndFeel::LookAndFeel()
{
    using namespace theme;

    setColourScheme ({ background, panel, control, outline, text, orange, background, controlHover, text });

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::buttonOnColourId, yellow);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::TextEditor::backgroundColourId, control);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, yellow.withAlpha (0.6f));
    setColour (juce::TextEditor::highlightColourId, yellow.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, yellow);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, yellow);
    setColour (juce::PopupMenu::highlightedTextColourId, background);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, outline);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, outline);
    setColour (juce::Slider::backgroundColourId, slot);
    setColour (juce::Slider::trackColourId, yellow);
    setColour (juce::Slider::thumbColourId, faderCap);
    setColour (juce::TooltipWindow::backgroundColourId, background);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, outline);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                    float sliderPos, float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style == juce::Slider::LinearHorizontal)
    {
        // small depth slider: track, fill from centre (bipolar) or left, cream thumb
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        auto track = area.withSizeKeepingCentre (area.getWidth(), 4.0f);
        g.setColour (theme::slot);
        g.fillRoundedRectangle (track, 2.0f);

        auto origin = slider.getProperties()["bipolar"] ? track.getCentreX() : track.getX();
        g.setColour (theme::yellow);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (origin, sliderPos), track.getY(),
                                                                juce::jmax (origin, sliderPos), track.getBottom()));

        g.setColour (slider.isMouseOverOrDragging() ? theme::faderCapTop : theme::faderCap);
        g.fillRoundedRectangle (juce::Rectangle<float> (sliderPos - 5.0f, area.getCentreY() - 8.0f, 10.0f, 16.0f), 2.0f);
        return;
    }

    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
        return;
    }

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    auto capHeight = 16.0f;
    auto track = bounds.withSizeKeepingCentre (6.0f, bounds.getHeight());

    g.setColour (theme::slot);
    g.fillRoundedRectangle (track, 3.0f);

    // tick marks, centre tick for bipolar faders
    g.setColour (theme::outline);
    auto top = bounds.getY() + capHeight * 0.5f, bottom = bounds.getBottom() - capHeight * 0.5f;

    for (int i = 0; i <= 10; ++i)
    {
        auto ty = juce::jmap ((float) i, 0.0f, 10.0f, top, bottom);
        auto w = (i == 0 || i == 10 || (i == 5 && slider.getProperties()["bipolar"])) ? 9.0f : 5.0f;
        g.fillRect (track.getX() - w - 2.0f, ty - 0.5f, w, 1.0f);
    }

    auto cap = juce::Rectangle<float> (bounds.getCentreX() - 15.0f, sliderPos - capHeight * 0.5f, 30.0f, capHeight);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 2.0f);
    g.setColour (slider.isMouseOverOrDragging() ? theme::faderCapTop : theme::faderCap);
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (theme::faderCapTop);
    g.fillRect (cap.removeFromTop (2.0f));
    g.setColour (theme::background.withAlpha (0.7f));
    g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 2.0f));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                        bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto colour = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : backgroundColour;

    if (isButtonDown)
        colour = colour.brighter (0.15f);
    else if (isMouseOverButton)
        colour = colour.brighter (0.07f);

    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.5f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 3.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return theme::label (juce::jmin (15.0f, (float) buttonHeight * 0.5f), true);
}

juce::Font LookAndFeel::getLabelFont (juce::Label& l)               { return theme::label (l.getFont().getHeight()); }
juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& box)       { return theme::label (juce::jmin (15.0f, (float) box.getHeight() * 0.55f)); }
juce::Font LookAndFeel::getPopupMenuFont()                          { return theme::label (15.0f); }
juce::Font LookAndFeel::getAlertWindowMessageFont()                 { return theme::label (15.0f); }
juce::Font LookAndFeel::getAlertWindowTitleFont()                   { return theme::label (18.0f, true); }

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height,
                                 bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver, bool)
{
    juce::Rectangle<int> thumb;

    if (isScrollbarVertical)
        thumb = { x + width / 2 - 3, thumbStartPosition, 6, thumbSize };
    else
        thumb = { thumbStartPosition, y + height / 2 - 3, thumbSize, 6 };

    g.setColour (bar.findColour (juce::ScrollBar::thumbColourId).brighter (isMouseOver ? 0.2f : 0.0f));
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

} // namespace sso
