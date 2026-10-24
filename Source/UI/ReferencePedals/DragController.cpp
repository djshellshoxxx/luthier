// OWNER: WP17
#include "DragController.h"
namespace luthier::reference
{
DragController::~DragController(){if(active())cancel();}
bool DragController::begin(const InstanceId& id,double now,bool live)
{
    if(active()&&!cancel())return false;
    const auto instance=model.getInstance(id);if(!instance)return false;
    const auto order=model.getOrder();candidate={(int)std::distance(order.begin(),std::find(order.begin(),order.end(),id)),instance->lane,instance->placement};
    baseline=candidate;identity=id;candidateSince=now;lastCommit=now-125;liveReorder=live;model.beginTransaction("Move pedal");return true;
}
bool DragController::update(Target target,double now)
{
    if(!active())return false;
    if(target!=candidate){candidate=target;candidateSince=now;}
    if(!liveReorder||now-candidateSince<80||now-lastCommit<125)return true;
    if(!commit(target))return false;lastCommit=now;return true;
}
bool DragController::commit(Target target)
{
    const auto instance=model.getInstance(identity);if(!instance){cancel();return false;}
    const auto order=model.getOrder();const int current=(int)std::distance(order.begin(),std::find(order.begin(),order.end(),identity));
    juce::String error;bool accepted=true;
    if(instance->placement!=target.placement)accepted=model.apply(cmd::SetPlacement{identity,target.placement},&error);
    if(accepted&&instance->lane!=target.lane)accepted=model.apply(cmd::SetLane{identity,target.lane},&error);
    if(accepted&&current!=target.index)accepted=model.apply(cmd::Move{identity,target.index},&error);
    if(!accepted){const auto reason=error;cancel();if(onError)onError(reason);}
    return accepted;
}
bool DragController::finish(Target target,double)
{
    if(!active()||!commit(target))return false;
    if(target==baseline)return cancel();
    identity=InstanceId::null();model.endTransaction();return true;
}
bool DragController::cancel()
{
    if(!active())return false;
    // Keep the identity until rollback succeeds; a compiler refusal must not
    // silently leave an open transaction behind.
    if(!model.cancelTransaction()){if(onError)onError("Unable to restore the drag baseline");return false;}
    identity=InstanceId::null();return true;
}
}
