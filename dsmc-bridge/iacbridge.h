#ifndef SPARTA_ISTHMUS_ABLATION_BRIDGE_H
#define SPARTA_ISTHMUS_ABLATION_BRIDGE_H

#include "isthmus_ablation/model.hpp"

namespace SPARTA_NS {

class SPARTA;

namespace IACBridge {

iac::Config &config();
iac::Model &model(SPARTA *sparta);
void reset_model();
void set_coupling_interval_from_dsmc(SPARTA *sparta);
void set_last_coupling_step(SPARTA *sparta);
void install_surface(SPARTA *sparta, const char *surface_id, int partflag, int type);

} // namespace IACBridge

} // namespace SPARTA_NS

#endif
