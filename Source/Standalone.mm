#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#if JUCE_MAC
// Defined in StandaloneNative_Mac.mm — isolated to avoid Cocoa/JUCE type conflicts
extern void nikaForceOpaqueWindow (void* nsViewHandle);
#endif

//==============================================================================
// Palette — mirrors PluginEditor constants exactly
//==============================================================================
static constexpr juce::uint32 kBg     = 0xFF080C08;
static constexpr juce::uint32 kTitleBg = 0xFF000000;  // pure black — title bar chrome
static constexpr juce::uint32 kBright = 0xFFB8C9A8;
static constexpr juce::uint32 kDim    = 0xFF3A4A3A;
static constexpr int          kLM     = 20;     // left margin — matches editor
static constexpr float        kFontSz = 13.0f;  // Courier New Bold body size
static constexpr int          kTitleH = 24;     // title bar height

//==============================================================================
class NIKATitleBarLAF : public juce::LookAndFeel_V4
{
public:
    //--------------------------------------------------------------------------
    void drawDocumentWindowTitleBar (juce::DocumentWindow&,
                                     juce::Graphics& g,
                                     int w, int h,
                                     int, int,
                                     const juce::Image*,
                                     bool) override
    {
        g.fillAll (juce::Colour (kTitleBg));
    }

    //--------------------------------------------------------------------------
    void fillResizableWindowBackground (juce::Graphics& g, int, int,
                                        const juce::BorderSize<int>&,
                                        juce::ResizableWindow&) override
    {
        g.fillAll (juce::Colour (kTitleBg));
    }

    //--------------------------------------------------------------------------
    void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                               const juce::Colour& bg, bool isOver, bool isDown) override
    {
        if (dynamic_cast<juce::DocumentWindow*> (btn.getParentComponent()) != nullptr)
            g.fillAll (juce::Colour (kTitleBg));
        else
            LookAndFeel_V4::drawButtonBackground (g, btn, bg, isOver, isDown);
    }

    //--------------------------------------------------------------------------
    void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                         bool isMouseOver, bool isDown) override
    {
        if (dynamic_cast<juce::DocumentWindow*> (btn.getParentComponent()) != nullptr)
        {
            g.setFont (juce::Font (juce::FontOptions ("Courier New", kFontSz, juce::Font::bold)));
            g.setColour (isMouseOver ? juce::Colour (kBright) : juce::Colour (kDim));
            auto text = btn.getButtonText().toUpperCase();
            // Multi-char (OPTIONS) → left-align to button edge so it lines up with kLM.
            // Single-char (X, -) → centre within the button rect.
            auto just = text.length() > 1 ? juce::Justification::centredLeft
                                           : juce::Justification::centred;
            g.drawText (text, btn.getLocalBounds(), just, false);
        }
        else
        {
            LookAndFeel_V4::drawButtonText (g, btn, isMouseOver, isDown);
        }
    }

    //--------------------------------------------------------------------------
    // Popup menu — matches title bar palette exactly
    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions ("Courier New", kFontSz, juce::Font::bold));
    }

    int getPopupMenuBorderSize() override { return 4; }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (kBg));
        g.setColour (juce::Colour (kDim));
        g.drawRect (0, 0, w, h, 1);
    }

    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool /*isTicked*/, bool /*hasSubMenu*/,
                            const juce::String& text, const juce::String& /*shortcut*/,
                            const juce::Drawable*, const juce::Colour*) override
    {
        if (isSeparator)
        {
            g.setColour (juce::Colour (kDim));
            g.drawLine (float (area.getX()),      float (area.getCentreY()),
                        float (area.getRight()),  float (area.getCentreY()), 1.0f);
            return;
        }

        if (isHighlighted && isActive)
        {
            // Invert — same as active plugin field
            g.fillAll (juce::Colour (kBright));
            g.setColour (juce::Colour (kBg));
        }
        else
        {
            g.setColour (isActive ? juce::Colour (kDim)
                                  : juce::Colour (kDim).withAlpha (0.35f));
        }

        g.setFont (juce::Font (juce::FontOptions ("Courier New", kFontSz, juce::Font::bold)));
        g.drawText (text.toUpperCase(),
                    area.withTrimmedLeft (kLM).withTrimmedRight (8),
                    juce::Justification::centredLeft, false);
    }

    //--------------------------------------------------------------------------
    // Plain ASCII only — avoids multi-byte encoding grief
    juce::Button* createDocumentWindowButton (int buttonType) override
    {
        juce::String label =
            buttonType == juce::DocumentWindow::closeButton    ? "x" :
            buttonType == juce::DocumentWindow::minimiseButton ? "-" : "";

        auto* btn = new juce::TextButton (label);
        btn->setColour (juce::TextButton::buttonColourId,   juce::Colour (kBg));
        btn->setColour (juce::TextButton::buttonOnColourId, juce::Colour (kBg));
        btn->setColour (juce::TextButton::textColourOffId,  juce::Colour (kDim));
        btn->setColour (juce::TextButton::textColourOnId,   juce::Colour (kBright));
        return btn;
    }
};

//==============================================================================
class NIKAStandaloneWindow : public juce::StandaloneFilterWindow
{
public:
    NIKAStandaloneWindow()
        : StandaloneFilterWindow ("НИКА",
                                  juce::Colour (kBg),
                                  nullptr,
                                  true)
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&laf_);
        setLookAndFeel (&laf_);
        setUsingNativeTitleBar (false);
        setTitleBarHeight (kTitleH);

        // Close + minimise on the right; no maximise
        setTitleBarButtonsRequired (juce::DocumentWindow::minimiseButton
                                    | juce::DocumentWindow::closeButton, false);

        // Fixed-size synth — no resize border needed (it would also inset the title bar)
        setResizable (false, false);
    }

    ~NIKAStandaloneWindow() override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    // Draw separator at true full window width — paintOverChildren is not clipped
    // by titleBarArea, unlike drawDocumentWindowTitleBar which runs inside that rect.
    void paintOverChildren (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (kDim));
        g.drawLine (0.0f, float (kTitleH) - 1.0f,
                    float (getWidth()), float (kTitleH) - 1.0f, 1.0f);
    }

    void resized() override
    {
        StandaloneFilterWindow::resized();

        // Pin OPTIONS button to left margin — aligns with plugin content x=kLM.
        // optionsButton is private in this JUCE build; find it as the only Button
        // that is a direct child of StandaloneFilterWindow.
        int btnH = kTitleH - 6;
        int btnY = (kTitleH - btnH) / 2;

        // OPTIONS — left edge at kLM; text is left-aligned so it starts at kLM
        for (auto* child : getChildren())
        {
            if (auto* btn = dynamic_cast<juce::Button*> (child))
            {
                btn->setBounds (kLM, btnY, 60, btnH);
                break;
            }
        }

        // Close (X) and minimise (-) — right edge flush with getWidth() - kLM,
        // matching where INIT / MONO end in the plugin body.
        const int btnW     = 18;
        const int gap      = 4;
        const int rightEdge = getWidth() - kLM;

        if (auto* close = getCloseButton())
            close->setBounds (rightEdge - btnW, btnY, btnW, btnH);

        if (auto* min = getMinimiseButton())
            min->setBounds (rightEdge - btnW * 2 - gap, btnY, btnW, btnH);
    }

private:
    NIKATitleBarLAF laf_;
};

//==============================================================================
class NIKAApplication : public juce::JUCEApplication
{
public:
    NIKAApplication() = default;

    const juce::String getApplicationName() override    { return "NIKA"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<NIKAStandaloneWindow>();
        mainWindow->setVisible (true);

#if JUCE_MAC
        // After setVisible() the peer and NSWindow exist — kill the macOS
        // compositor gradient that renders over custom JUCE title bars.
        if (auto* peer = mainWindow->getPeer())
            nikaForceOpaqueWindow (peer->getNativeHandle());
#endif
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();
        quit();
    }

private:
    std::unique_ptr<NIKAStandaloneWindow> mainWindow;
};

START_JUCE_APPLICATION (NIKAApplication)
