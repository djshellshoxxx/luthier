// OWNER: WP17
#pragma once
#include "../../ReferencePedals/BoardModel.h"
namespace luthier::reference
{
/** Message-thread gesture state. The model remains the sole persisted order. */
class DragController final
{
public:
    struct Target {
        int index=0,lane=0;Placement placement=Placement::beforeExistingPre;
        bool operator==(const Target& t) const {return index==t.index&&lane==t.lane&&placement==t.placement;}
        bool operator!=(const Target& t) const {return !(*this==t);}
    };
    explicit DragController(BoardModel& m):model(m){}
    ~DragController();
    bool begin(const InstanceId&,double now,bool live=true);
    bool update(Target,double now);
    bool finish(Target,double now);
    bool cancel();
    bool active() const {return !identity.isNull();}
    InstanceId id() const {return identity;}
    std::function<void(const juce::String&)> onError;
private:
    bool commit(Target);
    BoardModel& model;
    InstanceId identity=InstanceId::null();
    Target candidate;
    double candidateSince=0,lastCommit=-1000;
    bool liveReorder=true;
};
}
