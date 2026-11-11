#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <algorithm>

namespace BigVoixColors
{
    inline const juce::Colour leftBgCenter     { 0xff3B1054 };
    inline const juce::Colour leftBgEdge       { 0xff240836 };
    inline const juce::Colour neonHalo         { 0xff9333EA };

    inline const juce::Colour knobActiveTop    { 0xffA855F7 };
    inline const juce::Colour knobActiveBot    { 0xff7E22CE };
    inline const juce::Colour knobHighlight    { 0xffC084FC };
    inline const juce::Colour knobInactiveTop  { 0xff6B7280 };
    inline const juce::Colour knobInactiveBot  { 0xff4B5563 };

    inline const juce::Colour woodDarkBg       { 0xff1A0726 };
    inline const juce::Colour woodGrainLine    { 0xff240B36 };
    inline const juce::Colour woodBorder       { 0xff2E1042 };
    inline const juce::Colour waveformBg       { 0xff4C1D95 };

    inline const juce::Colour logoNeonCore     { 0xffD8B4FE };
    inline const juce::Colour lavenderAccent   { 0xffE9D5FF };
    inline const juce::Colour lavenderMuted    { 0xffA78BFA };
    inline const juce::Colour textDisabled     { 0xff6B7280 };

    inline const juce::Colour meterPurple      { 0xffA855F7 };
    inline const juce::Colour meterPink        { 0xffEC4899 };
    inline const juce::Colour clipRed          { 0xffF43F5E };
    inline const juce::Colour goldTooth        { 0xffFBBF24 };
    inline const juce::Colour tonguePink       { 0xffDB2777 };

    inline juce::Font makeFont(float height, bool bold = false, bool italic = false)
    {
       #if JUCE_MAJOR_VERSION >= 8
        auto opts = juce::FontOptions(height);
        if (bold && italic) opts = opts.withStyle("Bold Italic");
        else if (bold)      opts = opts.withStyle("Bold");
        else if (italic)    opts = opts.withStyle("Italic");
        return juce::Font(opts);
       #else
        int styleFlags = juce::Font::plain;
        if (bold)   styleFlags |= juce::Font::bold;
        if (italic) styleFlags |= juce::Font::italic;
        return juce::Font(height, styleFlags);
       #endif
    }
} // namespace BigVoixColors

class BigVoixKnob final : public juce::Slider
{
public:
    BigVoixKnob()
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setPopupDisplayEnabled(true, true, nullptr);
    }

    void configure(double defaultVal, bool bipolarEqMode = false, bool heroCompMode = false)
    {
        setDoubleClickReturnValue(true, defaultVal);
        getProperties().set("isBipolarEQ", bipolarEqMode);
        getProperties().set("isHeroComp", heroCompMode);
    }

    void setSectionMuted(bool shouldMute)
    {
        if (sectionMuted != shouldMute)
        {
            sectionMuted = shouldMute;
            getProperties().set("sectionMuted", shouldMute);
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            showDirectKeyboardInput();
            return;
        }
        juce::Slider::mouseDown(e);
    }

private:
    bool sectionMuted = false;
    std::unique_ptr<juce::Label> inlineEditor;

    void showDirectKeyboardInput()
    {
        inlineEditor = std::make_unique<juce::Label>("DirectInput", juce::String(getValue(), 1));
        inlineEditor->setBounds(getLocalBounds().withSizeKeepingCentre(std::max(48, getWidth() - 6), 22));
        inlineEditor->setColour(juce::Label::backgroundColourId, BigVoixColors::woodDarkBg);
        inlineEditor->setColour(juce::Label::textColourId, BigVoixColors::lavenderAccent);
        inlineEditor->setColour(juce::Label::outlineColourId, BigVoixColors::knobActiveTop);
        inlineEditor->setColour(juce::TextEditor::backgroundColourId, BigVoixColors::woodDarkBg);
        inlineEditor->setColour(juce::TextEditor::textColourId, BigVoixColors::lavenderAccent);
        inlineEditor->setColour(juce::TextEditor::highlightColourId, BigVoixColors::neonHalo);
        inlineEditor->setJustificationType(juce::Justification::centred);
        inlineEditor->setEditable(true, true, false);

        inlineEditor->onEditorHide = [this]()
        {
            if (inlineEditor != nullptr)
            {
                const juce::String rawText = inlineEditor->getText().retainCharacters("0123456789.-+");
                if (rawText.isNotEmpty())
                    setValue(rawText.getDoubleValue(), juce::sendNotificationSync);
                juce::MessageManager::callAsync([this]() { inlineEditor.reset(); });
            }
        };

        addAndMakeVisible(*inlineEditor);
        inlineEditor->showEditor();
    }
};

class BigVoixLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    BigVoixLookAndFeel()
    {
        using namespace BigVoixColors;
        setColour(juce::PopupMenu::backgroundColourId, woodDarkBg);
        setColour(juce::PopupMenu::textColourId, lavenderAccent);
        setColour(juce::PopupMenu::headerTextColourId, knobHighlight);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, neonHalo.withAlpha(0.45f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TooltipWindow::backgroundColourId, woodDarkBg);
        setColour(juce::TooltipWindow::textColourId, lavenderAccent);
        setColour(juce::TooltipWindow::outlineColourId, knobActiveTop);
    }

    ~BigVoixLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        using namespace BigVoixColors;

        const auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                 static_cast<float>(width), static_cast<float>(height)).reduced(2.0f);
        const float radius = std::min(area.getWidth(), area.getHeight()) * 0.5f;
        const auto centre  = area.getCentre();

        const bool isHeroComp   = slider.getProperties().getWithDefault("isHeroComp", false);
        const bool isBipolarEQ  = slider.getProperties().getWithDefault("isBipolarEQ", false);
        const bool sectionMuted = slider.getProperties().getWithDefault("sectionMuted", false);
        const bool alwaysActive = slider.getProperties().getWithDefault("alwaysActive", false);

        bool isKnobActive = !sectionMuted;
        if (isKnobActive && !isHeroComp && !alwaysActive)
        {
            if (isBipolarEQ)
                isKnobActive = (std::abs(slider.getValue()) > 0.08);
            else
                isKnobActive = (slider.getValue() > 0.15);
        }

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        if (isKnobActive && (isHeroComp || slider.isMouseOverOrDragging()))
        {
            const float glowR = radius * (isHeroComp ? 0.96f : 0.88f);
            juce::ColourGradient halo(neonHalo.withAlpha(isHeroComp ? 0.48f : 0.28f), centre.x, centre.y,
                                      neonHalo.withAlpha(0.0f), centre.x + glowR, centre.y, true);
            g.setGradientFill(halo);
            g.fillEllipse(centre.x - glowR, centre.y - glowR, glowR * 2.0f, glowR * 2.0f);
        }

        const int numTicks     = isHeroComp ? 31 : 19;
        const float tickOuterR = radius - 1.5f;
        const float tickInnerR = radius - (isHeroComp ? 7.5f : 5.0f);

        for (int i = 0; i < numTicks; ++i)
        {
            const float prop = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            const float tickAng = rotaryStartAngle + prop * (rotaryEndAngle - rotaryStartAngle);

            bool tickLit = false;
            if (isKnobActive)
            {
                if (isBipolarEQ)
                {
                    const float midProp = 0.5f;
                    tickLit = (sliderPosProportional >= midProp)
                            ? (prop >= midProp - 0.01f && prop <= sliderPosProportional + 0.01f)
                            : (prop <= midProp + 0.01f && prop >= sliderPosProportional - 0.01f);
                }
                else
                {
                    tickLit = (prop <= sliderPosProportional + 0.01f);
                }
            }

            const float sinA = std::sin(tickAng);
            const float cosA = -std::cos(tickAng);

            g.setColour(tickLit ? lavenderAccent : lavenderMuted.withAlpha(0.28f));
            g.drawLine(centre.x + sinA * tickInnerR, centre.y + cosA * tickInnerR,
                       centre.x + sinA * tickOuterR, centre.y + cosA * tickOuterR,
                       isHeroComp ? 1.8f : 1.3f);
        }

        const float bodyR = tickInnerR - (isHeroComp ? 6.0f : 3.5f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(centre.x - bodyR, centre.y - bodyR + 2.5f, bodyR * 2.0f, bodyR * 2.0f);

        const juce::Colour topCol = isKnobActive ? knobActiveTop : knobInactiveTop;
        const juce::Colour botCol = isKnobActive ? knobActiveBot : knobInactiveBot;

        juce::ColourGradient bodyGrad(topCol, centre.x, centre.y - bodyR,
                                      botCol, centre.x, centre.y + bodyR, false);
        g.setGradientFill(bodyGrad);
        g.fillEllipse(centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        g.setColour(isKnobActive ? knobHighlight.withAlpha(0.75f) : juce::Colours::white.withAlpha(0.22f));
        g.drawEllipse(centre.x - bodyR + 0.8f, centre.y - bodyR + 0.8f,
                      (bodyR - 0.8f) * 2.0f, (bodyR - 0.8f) * 2.0f, isHeroComp ? 2.2f : 1.4f);

        juce::Path pointerTriangle;
        const float triW   = isHeroComp ? 6.5f : 4.2f;
        const float triH   = isHeroComp ? 9.5f : 6.0f;
        const float triTop = -bodyR + (isHeroComp ? 5.0f : 3.0f);

        pointerTriangle.addTriangle(0.0f, triTop,
                                    -triW, triTop + triH,
                                     triW, triTop + triH);

        g.setColour(isKnobActive ? juce::Colours::white : lavenderAccent.withAlpha(0.85f));
        g.fillPath(pointerTriangle, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool /*shouldDrawButtonAsDown*/) override
    {
        using namespace BigVoixColors;
        const auto r = button.getLocalBounds().toFloat().reduced(1.5f);
        const bool toggled = button.getToggleState();

        // 1. Titres de sections cliquables (EQ, FX, DLY, REV) : toggled == true signifie Bypassé/Cut
        if (button.getProperties().getWithDefault("isSectionHeader", false))
        {
            const bool active = !toggled;
            g.setColour(active ? lavenderAccent : textDisabled);
            g.setFont(makeFont(22.0f, true, true));
            g.drawText(button.getButtonText(), r, juce::Justification::centred, false);

            if (!active)
            {
                g.setColour(clipRed.withAlpha(0.85f));
                g.drawLine(r.getX() + 6.0f, r.getCentreY(), r.getRight() - 6.0f, r.getCentreY(), 2.4f);
            }
            return;
        }

        // 2. Étiquettes cliquables des 4 réverbes (Short, Small, Big, Lush) : toggled == true signifie Cut
        if (button.getProperties().getWithDefault("isReverbCutLabel", false))
        {
            const bool active = !toggled;
            if (shouldDrawButtonAsHighlighted)
            {
                g.setColour(woodBorder.brighter(0.2f));
                g.fillRoundedRectangle(r, 4.0f);
            }
            g.setColour(active ? lavenderAccent : textDisabled);
            g.setFont(makeFont(12.5f, true));
            g.drawText(button.getButtonText(), r, juce::Justification::centred, false);
            if (!active)
            {
                g.setColour(clipRed.withAlpha(0.8f));
                g.drawLine(r.getX() + 10.0f, r.getCentreY(), r.getRight() - 10.0f, r.getCentreY(), 1.8f);
            }
            return;
        }

        // 3. Boutons standards (Low cut, Ping-pong) : toggled == true signifie Actif
        const juce::Colour bgCol = toggled ? knobActiveTop.withAlpha(0.32f) : woodDarkBg;
        const juce::Colour brCol = toggled ? knobActiveTop : woodBorder;

        g.setColour(bgCol);
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(shouldDrawButtonAsHighlighted ? knobHighlight : brCol);
        g.drawRoundedRectangle(r, 5.0f, 1.3f);

        g.setColour(toggled ? lavenderAccent : textDisabled);
        g.setFont(makeFont(12.0f, true));
        g.drawText(button.getButtonText(), r, juce::Justification::centred, false);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        using namespace BigVoixColors;
        const auto r = button.getLocalBounds().toFloat().reduced(1.2f);
        const bool isVioletTime = button.getProperties().getWithDefault("isVioletTimeBtn", false);

        const juce::Colour baseBg = isVioletTime ? knobActiveBot.withAlpha(0.55f) : woodDarkBg;
        g.setColour(shouldDrawButtonAsDown ? neonHalo.withAlpha(0.55f) : baseBg);
        g.fillRoundedRectangle(r, 5.0f);

        g.setColour(shouldDrawButtonAsHighlighted ? knobHighlight : (isVioletTime ? knobActiveTop : woodBorder));
        g.drawRoundedRectangle(r, 5.0f, 1.3f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool /*shouldDrawButtonAsDown*/) override
    {
        using namespace BigVoixColors;
        const auto r = button.getLocalBounds().toFloat();

        // Icône Disquette vectorielle pour le bouton Save
        if (button.getProperties().getWithDefault("isDiskIcon", false))
        {
            const auto ic = r.withSizeKeepingCentre(14.0f, 14.0f);
            g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : lavenderAccent);
            g.drawRoundedRectangle(ic, 2.0f, 1.5f);
            g.fillRect(ic.getX() + 3.0f, ic.getY(), ic.getWidth() - 6.0f, 5.0f);
            g.drawRect(ic.getX() + 3.0f, ic.getBottom() - 6.0f, ic.getWidth() - 6.0f, 6.0f, 1.2f);
            return;
        }

        // Icône Menu Hamburger vectorielle (3 barres horizontales)
        if (button.getProperties().getWithDefault("isBurgerIcon", false))
        {
            const auto ic = r.withSizeKeepingCentre(15.0f, 11.0f);
            g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : lavenderAccent);
            g.fillRoundedRectangle(ic.getX(), ic.getY(),              ic.getWidth(), 2.0f, 1.0f);
            g.fillRoundedRectangle(ic.getX(), ic.getCentreY() - 1.0f, ic.getWidth(), 2.0f, 1.0f);
            g.fillRoundedRectangle(ic.getX(), ic.getBottom() - 2.0f,  ic.getWidth(), 2.0f, 1.0f);
            return;
        }

        g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : lavenderAccent);
        g.setFont(makeFont(12.5f, true));
        g.drawText(button.getButtonText(), r, juce::Justification::centred, false);
    }

    void drawBubble(juce::Graphics& g, juce::BubbleComponent& comp,
                    const juce::Point<float>& tip, const juce::Rectangle<float>& body) override
    {
        (void)comp;
        (void)tip;
        using namespace BigVoixColors;
        g.setColour(woodDarkBg.withAlpha(0.95f));
        g.fillRoundedRectangle(body, 5.0f);
        g.setColour(knobActiveTop);
        g.drawRoundedRectangle(body, 5.0f, 1.4f);
    }

    juce::Font getSliderPopupFont(juce::Slider&) override
    {
        return BigVoixColors::makeFont(12.5f, true);
    }

    static void drawBeveledWoodStrip(juce::Graphics& g, const juce::Rectangle<float>& r, bool isSectionActive)
    {
        using namespace BigVoixColors;

        const float bevel = 14.0f;
        juce::Path stripPath;
        stripPath.startNewSubPath(r.getX() + bevel, r.getY());
        stripPath.lineTo(r.getRight() - 6.0f, r.getY());
        stripPath.quadraticTo(r.getRight(), r.getY(), r.getRight(), r.getY() + 6.0f);
        stripPath.lineTo(r.getRight(), r.getBottom() - 6.0f);
        stripPath.quadraticTo(r.getRight(), r.getBottom(), r.getRight() - 6.0f, r.getBottom());
        stripPath.lineTo(r.getX() + bevel, r.getBottom());
        stripPath.lineTo(r.getX(), r.getBottom() - bevel);
        stripPath.lineTo(r.getX(), r.getY() + bevel);
        stripPath.closeSubPath();

        g.setColour(woodDarkBg);
        g.fillPath(stripPath);

        g.saveState();
        g.reduceClipRegion(stripPath);
        g.setColour(woodGrainLine.withAlpha(0.65f));
        for (float ly = r.getY() + 5.0f; ly < r.getBottom(); ly += 7.5f)
        {
            juce::Path grain;
            grain.startNewSubPath(r.getX(), ly);
            grain.cubicTo(r.getX() + r.getWidth() * 0.33f, ly + 2.2f,
                          r.getX() + r.getWidth() * 0.66f, ly - 2.2f,
                          r.getRight(), ly + 0.8f);
            g.strokePath(grain, juce::PathStrokeType(1.1f));
        }
        g.restoreState();

        g.setColour(isSectionActive ? woodBorder.brighter(0.25f) : woodBorder.darker(0.3f));
        g.strokePath(stripPath, juce::PathStrokeType(1.6f));
    }

    static void drawCartoonCharacter(juce::Graphics& g,
                                     const juce::Rectangle<float>& leftPanelBounds,
                                     float compNorm,
                                     float compGrDb,
                                     bool isGlobalBypassed,
                                     juce::Path& outMouthHitPath)
    {
        using namespace BigVoixColors;

        const float x0 = leftPanelBounds.getX();
        const float y0 = leftPanelBounds.getY();
        const float w  = leftPanelBounds.getWidth();
        const float h  = leftPanelBounds.getHeight();

        const float intensity = isGlobalBypassed
                              ? 0.05f
                              : std::clamp(compNorm * 0.75f + (compGrDb / 18.0f) * 0.35f, 0.1f, 1.0f);

        juce::Path headShadow;
        headShadow.startNewSubPath(x0, y0 + h * 0.04f);
        headShadow.cubicTo(x0 + w * 0.38f, y0 + h * 0.08f,
                           x0 + w * 0.42f, y0 + h * 0.38f,
                           x0 + w * 0.34f, y0 + h * 0.88f);
        headShadow.lineTo(x0, y0 + h * 0.92f);
        headShadow.closeSubPath();
        g.setColour(leftBgEdge.darker(0.35f).withAlpha(0.55f));
        g.fillPath(headShadow);

        const float eyeCx = x0 + w * 0.19f;
        const float eyeCy = y0 + h * 0.25f;
        const float eyeRx = w * 0.115f;
        const float eyeRy = h * (0.075f + 0.020f * intensity);

        juce::Path eyeShape;
        eyeShape.addEllipse(eyeCx - eyeRx, eyeCy - eyeRy, eyeRx * 2.0f, eyeRy * 2.0f);

        g.setColour(isGlobalBypassed ? textDisabled : juce::Colours::white);
        g.fillPath(eyeShape);

        const float pupilR = eyeRx * (0.34f - 0.08f * intensity);
        g.setColour(leftBgEdge);
        g.fillEllipse(eyeCx + eyeRx * 0.18f - pupilR, eyeCy - pupilR, pupilR * 2.0f, pupilR * 2.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(eyeCx + eyeRx * 0.24f - pupilR * 0.35f, eyeCy - pupilR * 0.45f, pupilR * 0.6f, pupilR * 0.6f);

        juce::Path browPath;
        const float browTilt = 10.0f + 22.0f * intensity;
        browPath.startNewSubPath(eyeCx - eyeRx * 1.15f, eyeCy - eyeRy * 1.15f - browTilt * 0.4f);
        browPath.lineTo(eyeCx + eyeRx * 1.15f, eyeCy - eyeRy * 0.55f + browTilt * 0.5f);
        browPath.lineTo(eyeCx + eyeRx * 1.15f, eyeCy - eyeRy * 1.45f);
        browPath.lineTo(eyeCx - eyeRx * 1.15f, eyeCy - eyeRy * 1.65f);
        browPath.closeSubPath();
        g.setColour(leftBgCenter);
        g.fillPath(browPath);

        g.setColour(woodDarkBg);
        g.strokePath(eyeShape, juce::PathStrokeType(3.5f));
        g.drawLine(eyeCx - eyeRx * 1.1f, eyeCy - eyeRy * 1.05f - browTilt * 0.35f,
                   eyeCx + eyeRx * 1.12f, eyeCy - eyeRy * 0.48f + browTilt * 0.45f, 4.5f);

        const float mouthX = x0 + w * 0.02f;
        const float mouthY = y0 + h * 0.44f;
        const float mouthW = w * 0.36f;
        const float mouthH = h * (0.24f + 0.12f * intensity);

        outMouthHitPath.clear();
        outMouthHitPath.startNewSubPath(mouthX, mouthY + mouthH * 0.15f);
        outMouthHitPath.cubicTo(mouthX + mouthW * 0.45f, mouthY - mouthH * 0.10f,
                                mouthX + mouthW * 0.95f, mouthY + mouthH * 0.05f,
                                mouthX + mouthW, mouthY + mouthH * 0.42f);
        outMouthHitPath.cubicTo(mouthX + mouthW * 0.92f, mouthY + mouthH * 0.98f,
                                mouthX + mouthW * 0.35f, mouthY + mouthH * 1.05f,
                                mouthX, mouthY + mouthH * 0.85f);
        outMouthHitPath.closeSubPath();

        g.setColour(woodDarkBg.darker(0.5f));
        g.fillPath(outMouthHitPath);

        g.saveState();
        g.reduceClipRegion(outMouthHitPath);

        juce::Path tongue;
        tongue.addEllipse(mouthX + mouthW * 0.15f, mouthY + mouthH * 0.48f,
                          mouthW * 0.78f, mouthH * 0.65f);
        g.setColour(isGlobalBypassed ? knobInactiveBot : tonguePink);
        g.fillPath(tongue);

        const float toothW = mouthW / 4.2f;
        const float topToothH = mouthH * 0.28f;
        for (int t = 0; t < 4; ++t)
        {
            const juce::Rectangle<float> toothRect(mouthX + static_cast<float>(t) * toothW + 2.0f,
                                                   mouthY - 2.0f,
                                                   toothW - 3.0f,
                                                   topToothH);
            if (t == 2)
            {
                juce::ColourGradient shinyTooth(goldTooth, toothRect.getX(), toothRect.getY(),
                                                knobHighlight, toothRect.getRight(), toothRect.getBottom(), true);
                g.setGradientFill(shinyTooth);
                g.fillRoundedRectangle(toothRect, 4.0f);

                const float sx = toothRect.getCentreX();
                const float sy = toothRect.getCentreY();
                g.setColour(juce::Colours::white);
                g.drawLine(sx - 5.0f, sy, sx + 5.0f, sy, 1.6f);
                g.drawLine(sx, sy - 5.0f, sx, sy + 5.0f, 1.6f);
            }
            else
            {
                g.setColour(isGlobalBypassed ? textDisabled : juce::Colours::white);
                g.fillRoundedRectangle(toothRect, 4.0f);
            }
        }

        const float botToothH = mouthH * 0.24f;
        for (int t = 0; t < 4; ++t)
        {
            const juce::Rectangle<float> toothRect(mouthX + static_cast<float>(t) * toothW + 3.0f,
                                                   mouthY + mouthH - botToothH + 3.0f,
                                                   toothW - 3.0f,
                                                   botToothH);
            g.setColour(isGlobalBypassed ? textDisabled : juce::Colours::white.darker(0.05f));
            g.fillRoundedRectangle(toothRect, 4.0f);
        }

        g.restoreState();

        g.setColour(isGlobalBypassed ? knobInactiveTop : woodDarkBg);
        g.strokePath(outMouthHitPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};
