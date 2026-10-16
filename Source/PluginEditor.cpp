#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr int margin = 8;
    constexpr int topBarHeight = 56;
    constexpr int tabHeight = 26;
    constexpr int keyboardHeight = 62;
    const char* presetExtension = ".mrpreset";
}

void MeerumEditor::Content::paint (Graphics& g)
{
    g.fillAll (Theme::background);

    auto bar = getLocalBounds().removeFromTop (topBarHeight + margin).reduced (margin, 0).withTrimmedTop (margin);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (bar.toFloat(), 6.0f);
    g.setColour (Theme::outline);
    g.drawRoundedRectangle (bar.toFloat().reduced (0.5f), 6.0f, 1.0f);

    auto logo = bar.removeFromLeft (170).reduced (14, 0);
    g.setFont (Theme::font (24.0f, true));
    g.setGradientFill (ColourGradient (Theme::accent, (float) logo.getX(), 0.0f,
                                       Theme::accentAlt, (float) logo.getRight(), 0.0f, false));
    g.drawText ("MEERUM", logo, Justification::centredLeft, false);
}

MeerumEditor::MeerumEditor (MeerumProcessor& p)
    : AudioProcessorEditor (p),
      meerumProcessor (p),
      globalKnobs (p.apvts, content, { { "master", "MASTER" }, { "voices", "VOICES" }, { "glide", "GLIDE" },
                                       { "bendRange", "BEND" } }),
      legato (p.apvts, "legato", "LEGATO"),
      oscPage (p),
      fxPage (p.apvts),
      matrixPage (p.apvts),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    tooltips.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    presetBox.setJustificationType (Justification::centred);
    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (isPositiveAndBelow (index, (int) presets.size()))
            selectPreset (index);
    };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    saveButton.onClick = [this] { savePresetAs(); };
    saveButton.setTooltip ("Save the current sound as a user preset");
    themeButton.onClick = [this] { showThemePanel(); };
    themeButton.setTooltip ("Colours: dark/light mode and accent colour");

    for (auto* c : std::initializer_list<Component*> { &presetBox, &prevButton, &nextButton, &saveButton, &themeButton, &legato,
                                                       &pageTabs, &oscPage, &fxPage, &matrixPage, &keyboard })
        content.addAndMakeVisible (c);

    pageTabs.onChange = [this] (int index) { showPage (index); };
    showPage (0);

    keyboard.setKeyWidth (19.0f);
    keyboard.setLowestVisibleKey (24);
    keyboard.setWantsKeyboardFocus (false);

    refreshPresetList();
    refreshTheme();

    setResizable (true, true);
    setResizeLimits (baseWidth * 2 / 3, baseHeight * 2 / 3, baseWidth * 2, baseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth, baseHeight);

    startTimerHz (10);
}

MeerumEditor::~MeerumEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MeerumEditor::resized()
{
    content.setBounds (0, 0, baseWidth, baseHeight);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) baseWidth));
    layoutContent();
}

void MeerumEditor::layoutContent()
{
    auto r = content.getLocalBounds().reduced (margin);

    // Top bar: logo | preset browser | global knobs.
    auto bar = r.removeFromTop (topBarHeight);
    bar.removeFromLeft (170);
    auto presetArea = bar.removeFromLeft (360).withSizeKeepingCentre (360, 26);
    prevButton.setBounds (presetArea.removeFromLeft (26));
    presetArea.removeFromLeft (4);
    saveButton.setBounds (presetArea.removeFromRight (52));
    presetArea.removeFromRight (4);
    nextButton.setBounds (presetArea.removeFromRight (26));
    presetArea.removeFromRight (4);
    presetBox.setBounds (presetArea);

    bar.reduce (8, 3);
    themeButton.setBounds (bar.removeFromRight (64).withSizeKeepingCentre (64, 22));
    bar.removeFromRight (6);
    legato.setBounds (bar.removeFromRight (64).withSizeKeepingCentre (64, 22));
    bar.removeFromRight (8);
    auto knobs = bar.removeFromRight (4 * 58);
    layoutRow (knobs, { &globalKnobs[0], &globalKnobs[1], &globalKnobs[2], &globalKnobs[3] }, 2);

    r.removeFromTop (6);
    pageTabs.setBounds (r.removeFromTop (tabHeight).removeFromLeft (300));
    r.removeFromTop (6);

    keyboard.setBounds (r.removeFromBottom (keyboardHeight));
    r.removeFromBottom (6);

    for (auto* page : std::initializer_list<Component*> { &oscPage, &fxPage, &matrixPage })
        page->setBounds (r);
}

void MeerumEditor::showPage (int index)
{
    oscPage.setVisible (index == 0);
    fxPage.setVisible (index == 1);
    matrixPage.setVisible (index == 2);
}

// ---- presets ----------------------------------------------------------------
void MeerumEditor::refreshPresetList()
{
    presets.clear();
    presetBox.clear (dontSendNotification);

    for (int i = 0; i < meerumProcessor.getNumPrograms(); ++i)
        presets.push_back ({ meerumProcessor.getProgramName (i), i, {} });

    auto files = meerumProcessor.getPresetFolder().findChildFiles (File::findFiles, false, String ("*") + presetExtension);
    files.sort();
    for (auto& f : files)
        presets.push_back ({ f.getFileNameWithoutExtension(), -1, f });

    presetBox.addSectionHeading ("Factory");
    for (size_t i = 0; i < presets.size(); ++i)
    {
        if (presets[i].program < 0 && (i == 0 || presets[i - 1].program >= 0))
            presetBox.addSectionHeading ("User");
        presetBox.addItem (presets[i].name, (int) i + 1);
    }

    lastGeneration = -1; // force the name display to refresh
    timerCallback();
}

void MeerumEditor::selectPreset (int index)
{
    const auto& entry = presets[(size_t) index];
    if (entry.program >= 0)
        meerumProcessor.setCurrentProgram (entry.program);
    else if (! meerumProcessor.loadPreset (entry.file))
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Preset",
                                          "Could not load " + entry.file.getFileName());
}

void MeerumEditor::stepPreset (int delta)
{
    if (presets.empty())
        return;

    const auto current = meerumProcessor.getCurrentPresetName();
    int index = -1;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == current)
            index = (int) i;

    const int n = (int) presets.size();
    index = index < 0 ? (delta > 0 ? 0 : n - 1) : (index + delta + n) % n;
    selectPreset (index);
}

void MeerumEditor::savePresetAs()
{
    auto folder = meerumProcessor.getPresetFolder();
    folder.createDirectory();

    chooser = std::make_unique<FileChooser> ("Save preset",
                                             folder.getChildFile (meerumProcessor.getCurrentPresetName() + presetExtension),
                                             String ("*") + presetExtension);
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                              | FileBrowserComponent::warnAboutOverwriting,
                          [this] (const FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (file == File())
                                  return;

                              file = file.withFileExtension (presetExtension);
                              if (! meerumProcessor.savePreset (file))
                                  AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Preset",
                                                                    "Could not save " + file.getFullPathName());
                              refreshPresetList();
                          });
}

void MeerumEditor::showThemePanel()
{
    // Parented to the editor so it uses our LookAndFeel and stays inside the plugin window.
    juce::CallOutBox::launchAsynchronously (std::make_unique<ThemePanel>(),
                                            getLocalArea (&content, themeButton.getBounds()), this);
}

void MeerumEditor::refreshTheme()
{
    lastThemeGeneration = Theme::generation().load();
    lookAndFeel.refreshColours();

    const bool light = Theme::current().light;
    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId,            light ? Colours::white : Colour (0xffd9dee5));
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId,            Theme::blackKey);
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId,     Theme::outline);
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId,  Theme::accent.withAlpha (0.3f));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId,       Theme::accent);
    keyboard.setColour (MidiKeyboardComponent::shadowColourId,               Colours::transparentBlack);
    keyboard.setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, Theme::panelHeader);
    keyboard.setColour (MidiKeyboardComponent::upDownButtonArrowColourId,    Theme::accent);

    sendLookAndFeelChange(); // every child re-reads its colours and repaints
    repaint();
}

void MeerumEditor::timerCallback()
{
    if (Theme::generation().load() != lastThemeGeneration)
        refreshTheme();

    const int generation = meerumProcessor.presetGeneration.load();
    if (generation == lastGeneration)
        return;

    lastGeneration = generation;
    const auto name = meerumProcessor.getCurrentPresetName();
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == name)
        {
            presetBox.setSelectedId ((int) i + 1, dontSendNotification);
            return;
        }

    presetBox.setText (name, dontSendNotification);
}
