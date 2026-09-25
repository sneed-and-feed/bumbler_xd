#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../ui/BumblerLookAndFeel.h"
#include "../ui/LCDGraphDisplay.h"
#include "../ui/HorizontalFader.h"
#include "PresetMigrator.h"
#include <memory>
#include <vector>

namespace bumbler {

class BumblerAudioProcessor;

class BumblerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::AudioProcessorValueTreeState::Listener,
                                    public juce::FileDragAndDropTarget,
                                    public juce::Timer {
public:
    explicit BumblerAudioProcessorEditor(BumblerAudioProcessor&);
    ~BumblerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // juce::FileDragAndDropTarget interface
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // juce::Timer interface (toast animation)
    void timerCallback() override;

    void refreshPresetMenu();
    void showMigrateMenu(juce::Point<int> screenPos);
    void migrateFilesAndLoad(const juce::Array<juce::File>& files);
    void showStatusToast(const juce::String& message, bool isError = false);

    // Component slot helper structures
    struct KnobSlot {
        juce::String paramId;
        juce::Slider slider;
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct ButtonSlot {
        juce::String paramId;
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    struct ComboSlot {
        juce::String paramId;
        juce::Label label;
        juce::ComboBox comboBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

private:
    void setupUI();
    void updateLcdDisplays();

    void addKnob(const juce::ParameterID& paramId, const char* name,
                 juce::Slider::SliderStyle style = juce::Slider::RotaryHorizontalVerticalDrag,
                 const char* suffix = "");
    void addButton(const juce::ParameterID& paramId, const char* text);
    void addCombo(const juce::ParameterID& paramId, const char* labelText, const juce::StringArray& choices);

    KnobSlot*   findKnob(const juce::String& paramId);
    ButtonSlot* findButton(const juce::String& paramId);
    ComboSlot*  findCombo(const juce::String& paramId);

    void showKnobContextMenu(KnobSlot& slot, juce::Point<int> screenPos);
    void showFaderContextMenu(juce::Point<int> screenPos);

    BumblerAudioProcessor& mProcessor;
    BumblerLookAndFeel     mLookAndFeel;

    // Specialized Custom Components
    HorizontalFader  mOscMixFader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mOscMixAttachment;

    LCDGraphDisplay  mFilterLcdDisplay { LCDGraphDisplay::DisplayType::FilterEnvelope };
    LCDGraphDisplay  mAmpLcdDisplay    { LCDGraphDisplay::DisplayType::AmpEnvelope };

    // Header preset selector & migrator
    juce::Label      mPresetLabel;
    juce::ComboBox   mPresetComboBox;
    juce::TextButton mMigrateButton { "MIGRATE..." };
    std::unique_ptr<juce::FileChooser> mFileChooser;

    // Toast and drag-and-drop state
    bool         mIsDraggingFiles { false };
    juce::String mToastMessage;
    bool         mToastIsError { false };
    float        mToastAlpha { 0.0f };
    int          mToastCountdown { 0 };

    struct UserPresetItem {
        juce::String name;
        juce::String category;
        juce::File file;
    };
    std::vector<UserPresetItem> mUserPresets;

    // Slot vectors for RAII parameter attachment management
    std::vector<std::unique_ptr<KnobSlot>>   mKnobs;
    std::vector<std::unique_ptr<ButtonSlot>> mButtons;
    std::vector<std::unique_ptr<ComboSlot>>  mCombos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BumblerAudioProcessorEditor)
};

} // namespace bumbler
