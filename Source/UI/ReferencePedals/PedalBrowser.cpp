// OWNER: WP22
#include "PedalBrowser.h"
#include "../Theme.h"
namespace luthier::reference
{
PedalBrowser::PedalBrowser(BoardModel& m):model(m)
{
    for(auto* c:std::array<juce::Component*,4>{&search,&category,&list,&add})addAndMakeVisible(c);
    search.setTextToShowWhenEmpty("Search name / family",Palette::textMuted);search.setTitle("Search pedals");search.onTextChange=[this]{refresh();};
    category.addItem("All pedals",1);category.addItem("Generic",2);category.addItem("Reference",3);category.setSelectedId(1);category.onChange=[this]{refresh();};
    list.setRowHeight(62);list.setMultipleSelectionEnabled(false);add.onClick=[this]{addSelected();};refresh();
}
void PedalBrowser::refresh()
{
    const auto selected=list.getSelectedRow();items.clear();const auto q=search.getText().trim();
    for(const auto* d:PedalRegistry::all())
    {
        const juce::String text=juce::String(d->displayName)+" "+d->family+" "+(d->inspiredBy?d->inspiredBy:"");
        const bool generic=juce::String(d->modelId).startsWith("generic.");
        if((q.isEmpty()||text.containsIgnoreCase(q))&&(category.getSelectedId()!=2||generic)&&(category.getSelectedId()!=3||!generic))items.push_back(d->modelId);
    }
    list.updateContent();if(!items.empty())list.selectRow(juce::jlimit(0,(int)items.size()-1,selected));
    add.setEnabled(model.getInstanceCount()<kMaxSlots&&!items.empty());add.setTooltip(model.getInstanceCount()>=kMaxSlots?"Board capacity is 16 pedals. Remove a pedal first.":"Add at the end of the signal chain");
}
void PedalBrowser::resized(){auto r=getLocalBounds().reduced(6);search.setBounds(r.removeFromTop(28));category.setBounds(r.removeFromTop(28));add.setBounds(r.removeFromBottom(32));list.setBounds(r);}
void PedalBrowser::paintListBoxItem(int n,juce::Graphics& g,int w,int h,bool selected)
{
    if(n<0||n>=(int)items.size())return;const auto* d=PedalRegistry::find(items[(size_t)n].c_str());if(!d)return;
    g.fillAll(selected?Palette::accentDim:Palette::panel);g.setColour(Palette::textPrimary);g.setFont(13.f);g.drawFittedText(d->displayName,6,3,w-12,24,juce::Justification::centredLeft,2);
    g.setFont(10.f);g.setColour(Palette::textMuted);g.drawFittedText(d->inspiredBy?"Inspired by "+juce::String(d->inspiredBy):juce::String(d->family),6,29,w-12,15,juce::Justification::centredLeft,1);
    g.drawText("EST | "+juce::String(d->numParams)+" controls",6,h-17,w-12,14,juce::Justification::centredLeft);
}
juce::var PedalBrowser::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    if(rows.size()==0)return {};const int row=rows[0];return row>=0&&row<(int)items.size()?juce::var("ref-pedal:"+juce::String(items[(size_t)row])):juce::var();
}
void PedalBrowser::addSelected(){const auto n=list.getSelectedRow();if(n>=0&&n<(int)items.size())model.apply(cmd::Add{items[(size_t)n]});refresh();}
void PedalBrowser::listBoxItemDoubleClicked(int n,const juce::MouseEvent&){list.selectRow(n);addSelected();}
void PedalBrowser::returnKeyPressed(int n){list.selectRow(n);addSelected();}
}
