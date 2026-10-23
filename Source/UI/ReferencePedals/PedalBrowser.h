// OWNER: WP22
#pragma once
#include "../../ReferencePedals/BoardModel.h"
#include "../../ReferencePedals/PedalRegistry.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
class PedalBrowser final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PedalBrowser(BoardModel&);
    void resized() override;
    void refresh();
    void focusSearch(){search.grabKeyboardFocus();}
private:
    int getNumRows() override {return (int)items.size();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override;
    void returnKeyPressed(int) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rows) override;
    void addSelected();
    BoardModel& model;
    juce::TextEditor search;
    juce::ComboBox category;
    juce::ListBox list {"Pedal browser",this};
    juce::TextButton add {"Add selected pedal"};
    std::vector<std::string> items;
};
}
